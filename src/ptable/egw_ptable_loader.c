#include "egw_ptable_loader.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

struct egw_ptable {
    egw_bin_header_t header;
    void            *map;
    size_t           map_size;
    void            *entries;
    int              fd;
};

egw_err_t egw_ptable_open(const char *path, uint32_t expected_magic,
                           egw_ptable_t **out)
{
    if (!path || !out) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return EGW_ERR_NOTFOUND;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || (size_t)st.st_size < sizeof(egw_bin_header_t)) {
        close(fd);
        return EGW_ERR_PARSE;
    }

    void *map = mmap(NULL, (size_t)st.st_size, PROT_READ,
                     MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return EGW_ERR_NOMEM;
    }

    const egw_bin_header_t *hdr = (const egw_bin_header_t *)map;

    if (hdr->magic != expected_magic || hdr->endianness != EGW_BIN_ENDIAN_LITTLE) {
        munmap(map, (size_t)st.st_size);
        close(fd);
        return EGW_ERR_PARSE;
    }

    egw_ptable_t *pt = calloc(1, sizeof(*pt));
    if (!pt) {
        munmap(map, (size_t)st.st_size);
        close(fd);
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    memcpy(&pt->header, hdr, sizeof(pt->header));
    pt->map = map;
    pt->map_size = (size_t)st.st_size;
    pt->fd = fd;
    pt->entries = (uint8_t *)map + sizeof(egw_bin_header_t);

    *out = pt;
    return EGW_OK;
}

void egw_ptable_close(egw_ptable_t *pt)
{
    if (!pt) {
        return;
    }

    munmap(pt->map, pt->map_size);
    close(pt->fd);
    free(pt);
}

const void *egw_ptable_entries(egw_ptable_t *pt)
{
    return pt ? pt->entries : NULL;
}

uint32_t egw_ptable_entry_count(egw_ptable_t *pt)
{
    return pt ? pt->header.entry_count : 0;
}

uint32_t egw_ptable_build_id(egw_ptable_t *pt)
{
    return pt ? pt->header.build_id : 0;
}
