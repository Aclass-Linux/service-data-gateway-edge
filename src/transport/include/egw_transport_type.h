#ifndef EGW_TRANSPORT_TYPE_H
#define EGW_TRANSPORT_TYPE_H

#include "egw_defs.h"
#include <stddef.h>
#include <stdint.h>

struct egw_transport {
    egw_err_t (*open)(const void *params, int *out_fd);
    void      (*close)(int fd);
    egw_err_t (*read)(int fd, void *buf, size_t *out_len, size_t cap);
    egw_err_t (*write)(int fd, const void *data, size_t *out_written, size_t len);
};

#endif /* EGW_TRANSPORT_TYPE_H */
