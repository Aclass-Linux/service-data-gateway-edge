#include "egw_persist_thread.h"
#include "egw_persist.h"
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <stdio.h>

#define EGW_PERSIST_ITEM_SIZE 32

typedef struct {
    char     table[16];
    uint16_t device_id;
    uint16_t reg_addr;
    uint16_t value;
    uint8_t  _pad[10];
} egw_persist_item_t;

struct egw_persist_thread {
    egw_persist_t *persist;
    int           pipe_fd[2];
    int           interval_ms;
    volatile int  running;
    char          db_path[256];
};

egw_persist_thread_t *egw_persist_thread_create(const char *db_path,
                                                  int interval_ms)
{
    egw_persist_thread_t *pt = calloc(1, sizeof(*pt));
    if (!pt) { return NULL; }

    if (pipe(pt->pipe_fd) != 0) {
        free(pt);
        return NULL;
    }
    snprintf(pt->db_path, sizeof(pt->db_path), "%s", db_path);
    pt->interval_ms = interval_ms;
    pt->running     = 1;

    return pt;
}

void *egw_persist_thread_fn(void *arg)
{
    egw_persist_thread_t *pt = arg;

    pt->persist = egw_persist_open(pt->db_path);
    if (!pt->persist) {
        fprintf(stderr, "persist_thread: open failed\n");
        return NULL;
    }

    /* 首次全量快照 */
    egw_persist_begin(pt->persist);
    egw_persist_full_dump(pt->persist, "southbound");
    egw_persist_full_dump(pt->persist, "northbound");
    egw_persist_commit(pt->persist);

    struct pollfd pfd = { .fd = pt->pipe_fd[0], .events = POLLIN };

    while (pt->running) {
        int ret = poll(&pfd, 1, pt->interval_ms);
        if (ret < 0) { break; }

        egw_persist_begin(pt->persist);

        if (ret > 0 && (pfd.revents & POLLIN)) {
            egw_persist_item_t buf[64];
            for (;;) {
                ssize_t n = read(pt->pipe_fd[0], buf, sizeof(buf));
                if (n <= 0) { break; }
                size_t count = n / sizeof(egw_persist_item_t);
                for (size_t i = 0; i < count; i++) {
                    egw_persist_put(pt->persist, buf[i].table,
                                     buf[i].device_id, buf[i].reg_addr,
                                     buf[i].value);
                }
            }
        }

        egw_persist_commit(pt->persist);
    }

    /* 最后一次落盘 */
    egw_persist_begin(pt->persist);
    {
        egw_persist_item_t buf[64];
        for (;;) {
            ssize_t n = read(pt->pipe_fd[0], buf, sizeof(buf));
            if (n <= 0) { break; }
            size_t count = n / sizeof(egw_persist_item_t);
            for (size_t i = 0; i < count; i++) {
                egw_persist_put(pt->persist, buf[i].table,
                                 buf[i].device_id, buf[i].reg_addr,
                                 buf[i].value);
            }
        }
    }
    egw_persist_commit(pt->persist);

    egw_persist_close(pt->persist);
    pt->persist = NULL;
    return NULL;
}

void egw_persist_thread_enqueue(egw_persist_thread_t *pt,
                                 const char *table,
                                 uint16_t device_id,
                                 uint16_t reg_addr,
                                 uint16_t value)
{
    if (!pt || !pt->running) { return; }

    egw_persist_item_t item;
    memset(&item, 0, sizeof(item));
    snprintf(item.table, sizeof(item.table), "%s", table);
    item.table[sizeof(item.table) - 1] = '\0';
    item.device_id = device_id;
    item.reg_addr  = reg_addr;
    item.value     = value;

    write(pt->pipe_fd[1], &item, sizeof(item));
}

void egw_persist_thread_request_stop(egw_persist_thread_t *pt)
{
    if (!pt) { return; }
    pt->running = 0;
    close(pt->pipe_fd[1]);
}

void egw_persist_thread_destroy(egw_persist_thread_t *pt)
{
    if (!pt) { return; }
    close(pt->pipe_fd[0]);
    close(pt->pipe_fd[1]);
    free(pt);
}
