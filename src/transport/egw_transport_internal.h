#ifndef EGW_TRANSPORT_INTERNAL_H
#define EGW_TRANSPORT_INTERNAL_H

/* 内部实现定义 — 仅供 transport 模块 .c 文件包含，不对外暴露 */

#include "egw_transport.h"

typedef struct egw_transport_handle {
    int fd;
    egw_err_t (*read)(struct egw_transport_handle *h, void *buf,
                       size_t *out_len, size_t cap);
    egw_err_t (*write)(struct egw_transport_handle *h, const void *data,
                        size_t *out_written, size_t len);
    void      (*close)(struct egw_transport_handle *h);
} egw_transport_handle_t;

#endif /* EGW_TRANSPORT_INTERNAL_H */
