#ifndef EGW_TRANSPORT_H
#define EGW_TRANSPORT_H

#include "egw_defs.h"
#include <stddef.h>
#include <stdint.h>

/* ── 不透明句柄 ─────────────────────────────────── */

typedef struct egw_transport_instance egw_transport_instance_t;
typedef struct egw_serial            egw_serial_t;

/* ── 回调类型 ──────────────────────────────────── */

typedef void (*egw_transport_open_cb)(void *tp, egw_err_t err);
typedef void (*egw_transport_data_cb)(void *tp, const void *buf, size_t len);
typedef void (*egw_transport_write_cb)(void *tp, egw_err_t err);
typedef void (*egw_transport_close_cb)(void *tp, egw_err_t err);

typedef struct egw_transport_cbs {
    egw_transport_open_cb   on_open;
    egw_transport_data_cb   on_data;
    egw_transport_write_cb  on_write;
    egw_transport_close_cb  on_close;
    void                   *user_data;
} egw_transport_cbs_t;

/* ── 实例生命周期 ───────────────────────────────── */

egw_transport_instance_t *egw_transport_create(void);

void egw_transport_destroy(egw_transport_instance_t *inst);

/* ── 事件循环（阻塞，当前线程）───────────────────── */

void egw_transport_run(egw_transport_instance_t *inst);

void egw_transport_stop(egw_transport_instance_t *inst);

/* ── 多态宏 ──────────────────────────────────── */

uint32_t egw_transport_get_id(const void *tp);
egw_transport_instance_t *egw_transport_get_inst(const void *tp);

/* ── 底层循环访问（信号处理等）────────────────── */

void *egw_transport_get_loop(egw_transport_instance_t *inst);

#define egw_close(tp)  _Generic((tp),                    \
    egw_serial_t *: egw_serial_close)(tp)

#define egw_write(tp, buf, len) _Generic((tp),           \
    egw_serial_t *: egw_serial_write)(tp, buf, len)

#endif /* EGW_TRANSPORT_H */
