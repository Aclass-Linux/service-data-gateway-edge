#ifndef EGW_TRANSPORT_H
#define EGW_TRANSPORT_H

#include "egw_defs.h"
#include <stddef.h>
#include <stdint.h>

typedef struct egw_transport_handle egw_transport_handle_t;

/* ── 通用 I/O ──────────────────────────────────────────── */

egw_err_t egw_transport_read(egw_transport_handle_t *h, void *buf,
                              size_t *out_len, size_t cap);
egw_err_t egw_transport_write(egw_transport_handle_t *h, const void *data,
                               size_t *out_written, size_t len);
void egw_transport_close(egw_transport_handle_t *h);
int egw_transport_get_fd(const egw_transport_handle_t *h);

/* ── 串口 ──────────────────────────────────────────────── */

typedef struct egw_transport_serial_params {
    const char *path;
    int32_t     baud;
    char        parity;
    int32_t     data_bits;
    int32_t     stop_bits;
} egw_transport_serial_params_t;

egw_transport_handle_t *egw_transport_serial_open(const egw_transport_serial_params_t *params);

/* ── TCP ───────────────────────────────────────────────── */

typedef struct egw_transport_tcp_params {
    const char *host;
    uint16_t    port;
    uint32_t    connect_timeout_ms;
} egw_transport_tcp_params_t;

egw_transport_handle_t *egw_transport_tcp_open(const egw_transport_tcp_params_t *params);

/* ── 全大写宏 ──────────────────────────────────────────── */

#define EGW_TRANSPORT_READ(h, buf, out_len, cap) \
    egw_transport_read((h), (buf), (out_len), (cap))

#define EGW_TRANSPORT_WRITE(h, data, out_written, len) \
    egw_transport_write((h), (data), (out_written), (len))

#define EGW_TRANSPORT_CLOSE(h) egw_transport_close(h)
#define EGW_TRANSPORT_GET_FD(h) egw_transport_get_fd(h)

#define EGW_TRANSPORT_OPEN(params)                                              \
    _Generic((params),                                                           \
            const egw_transport_serial_params_t *: egw_transport_serial_open,     \
            egw_transport_serial_params_t *: egw_transport_serial_open,          \
            const egw_transport_tcp_params_t *: egw_transport_tcp_open,          \
            egw_transport_tcp_params_t *: egw_transport_tcp_open                 \
    )(params)

#endif /* EGW_TRANSPORT_H */
