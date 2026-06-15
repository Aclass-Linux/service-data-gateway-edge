#include "egw_persist.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define SLOT_SIZE EGW_PERSIST_SLOT_SIZE
#define PAGE_SIZE EGW_PERSIST_PAGE_SIZE

/* ── seqlock 槽位（persist 文件内和内存共用布局）───────── */

typedef struct {
    uint32_t gen;
    uint8_t  pad[4];
    uint64_t raw;
} egw_live_slot_t;

_Static_assert(sizeof(egw_live_slot_t) == SLOT_SIZE, "slot size mismatch");

/* ── 文件头 ──────────────────────────────────────────── */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t build_id;
    uint32_t checksum;
    uint64_t last_flush_unix_ms;
    uint32_t slot_count;
    uint32_t pad;
} egw_persist_header_t;

#define PERSIST_MAGIC   0x50565354u  /* "PVST" */
#define PERSIST_VERSION 1u

/* ── 运行时结构 ──────────────────────────────────────── */

struct egw_persist {
    char             *path;
    int               fd;
    size_t            file_size;
    egw_live_slot_t  *slots;
    uint8_t          *dirty_bitmap;
    uint32_t          slot_count;
    uint32_t          dirty_words;
};

static inline void set_dirty(egw_persist_t *p, uint32_t slot)
{
    uint32_t page = (slot * SLOT_SIZE) / PAGE_SIZE;
    uint32_t word = page / 32u;
    uint32_t bit  = page % 32u;

    p->dirty_bitmap[word] |= (uint8_t)(1u << bit);
}

/* ── 生命周期 ────────────────────────────────────────── */

egw_persist_t *egw_persist_create(const char *file_path, uint32_t slot_count)
{
    if (!file_path || slot_count == 0) {
        return NULL;
    }

    egw_persist_t *p = calloc(1, sizeof(*p));
    if (!p) {
        return NULL;
    }

    p->path = strdup(file_path);
    p->slot_count = slot_count;

    size_t data_size  = (size_t)slot_count * SLOT_SIZE;
    size_t total_size = sizeof(egw_persist_header_t) + data_size;

    uint32_t total_pages = (uint32_t)((data_size + PAGE_SIZE - 1u) / PAGE_SIZE);
    p->dirty_words = (total_pages + 31u) / 32u;

    int fd = open(file_path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        free(p->path);
        free(p);
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        free(p->path);
        free(p);
        return NULL;
    }

    if ((size_t)st.st_size < total_size) {
        if (ftruncate(fd, (off_t)total_size) != 0) {
            close(fd);
            free(p->path);
            free(p);
            return NULL;
        }
    }

    void *map = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        free(p->path);
        free(p);
        return NULL;
    }

    egw_persist_header_t *hdr = (egw_persist_header_t *)map;
    if (hdr->magic == 0) {
        hdr->magic      = PERSIST_MAGIC;
        hdr->version    = PERSIST_VERSION;
        hdr->slot_count = slot_count;
    }

    p->fd        = fd;
    p->file_size = total_size;
    p->slots     = (egw_live_slot_t *)((uint8_t *)map + sizeof(egw_persist_header_t));

    p->dirty_bitmap = calloc(p->dirty_words, 1);
    if (!p->dirty_bitmap) {
        munmap(map, total_size);
        close(fd);
        free(p->path);
        free(p);
        return NULL;
    }

    return p;
}

void egw_persist_destroy(egw_persist_t *p)
{
    if (!p) {
        return;
    }

    egw_persist_flush(p);

    munmap((void *)((uint8_t *)p->slots - sizeof(egw_persist_header_t)),
           p->file_size);
    close(p->fd);
    free(p->dirty_bitmap);
    free(p->path);
    free(p);
}

/* ── 主回路更新值 ────────────────────────────────────── */

void egw_persist_set(egw_persist_t *p, uint32_t slot, egw_value_t value)
{
    if (!p || slot >= p->slot_count) {
        return;
    }

    egw_live_slot_t *s = &p->slots[slot];

    s->gen ^= 1u;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    s->raw = value.raw;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    s->gen ^= 1u;

    set_dirty(p, slot);
}

/* ── 落盘 ────────────────────────────────────────────── */

void egw_persist_flush(egw_persist_t *p)
{
    if (!p) {
        return;
    }

    egw_persist_header_t *hdr = (egw_persist_header_t *)
        ((uint8_t *)p->slots - sizeof(egw_persist_header_t));

    uint32_t slots_per_page = PAGE_SIZE / SLOT_SIZE;

    for (uint32_t w = 0; w < p->dirty_words; w++) {
        uint8_t bits = p->dirty_bitmap[w];
        if (bits == 0) {
            continue;
        }

        for (int b = 0; b < 8; b++) {
            if ((bits & (1u << b)) == 0) {
                continue;
            }

            uint32_t page   = w * 32u + (uint32_t)b;
            uint32_t offset = page * PAGE_SIZE;

            if (offset + PAGE_SIZE <= p->file_size - sizeof(egw_persist_header_t)) {
                /* seqlock 读每个槽位 */
                for (uint32_t s = 0; s < slots_per_page; s++) {
                    uint32_t slot = page * slots_per_page + s;
                    if (slot >= p->slot_count) {
                        break;
                    }

                    egw_live_slot_t *sl = &p->slots[slot];
                    uint32_t g;
                    uint64_t v;

                    for (int retry = 0; retry < 3; retry++) {
                        g = __atomic_load_n(&sl->gen, __ATOMIC_ACQUIRE);
                        if (g & 1u) {
                            continue;
                        }
                        v = __atomic_load_n(&sl->raw, __ATOMIC_ACQUIRE);
                        if (g == __atomic_load_n(&sl->gen, __ATOMIC_ACQUIRE)) {
                            sl->raw = v;
                            break;
                        }
                    }
                }

                void *page_ptr = (uint8_t *)p->slots + offset;
                pwrite(p->fd, page_ptr, PAGE_SIZE,
                       (off_t)(sizeof(egw_persist_header_t) + offset));
            }
        }

        p->dirty_bitmap[w] = 0;
    }

    hdr->last_flush_unix_ms = 0;
    fdatasync(p->fd);
}

/* ── 读取值 ──────────────────────────────────────────── */

egw_value_t egw_persist_get(egw_persist_t *p, uint32_t slot)
{
    egw_value_t v = { .raw = 0 };

    if (!p || slot >= p->slot_count) {
        return v;
    }

    egw_live_slot_t *s = &p->slots[slot];
    uint32_t g;
    uint64_t raw;

    for (int retry = 0; retry < 3; retry++) {
        g = __atomic_load_n(&s->gen, __ATOMIC_ACQUIRE);
        if (g & 1u) {
            continue;
        }
        raw = __atomic_load_n(&s->raw, __ATOMIC_ACQUIRE);
        if (g == __atomic_load_n(&s->gen, __ATOMIC_ACQUIRE)) {
            v.raw = raw;
            return v;
        }
    }

    v.raw = s->raw;
    return v;
}
