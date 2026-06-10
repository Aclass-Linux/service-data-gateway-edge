#ifndef EGW_TRANSPORT_H
#define EGW_TRANSPORT_H

#include "egw_defs.h"
#include "egw_serial_params.h"
#include <stddef.h>
#include <stdint.h>

/* ── 不透明句柄 ─────────────────────────────────── */

typedef struct egw_transport            egw_transport_t;
typedef struct egw_transport_instance   egw_transport_instance_t;

/* ── 回调类型 ──────────────────────────────────── */

typedef void (*egw_transport_open_cb)(egw_transport_t *tp, egw_err_t err);
typedef void (*egw_transport_data_cb)(egw_transport_t *tp, const void *buf, size_t len);
typedef void (*egw_transport_write_cb)(egw_transport_t *tp, egw_err_t err);
typedef void (*egw_transport_close_cb)(egw_transport_t *tp, egw_err_t err);

typedef struct egw_transport_cbs {
    egw_transport_open_cb   on_open;
    egw_transport_data_cb   on_data;
    egw_transport_write_cb  on_write;
    egw_transport_close_cb  on_close;
    void                   *user_data;
} egw_transport_cbs_t;

/* ── Transport 类型 ──────────────────────────────── */

typedef enum {
    EGW_TRANSPORT_SERIAL,
} egw_transport_type_t;

/* ── 创建配置 ──────────────────────────────────── */

typedef struct egw_transport_cfg {
    egw_transport_type_t type;
    egw_transport_cbs_t  cbs;

    union {
        egw_serial_params_t serial;
    };
} egw_transport_cfg_t;

/* ── 实例生命周期（app 主线程调用）───────────────── */

egw_transport_instance_t *egw_transport_create(void);

egw_err_t egw_transport_register(egw_transport_instance_t *inst,
                                  const egw_transport_cfg_t *cfg,
                                  egw_transport_t **tp);

void egw_transport_destroy(egw_transport_instance_t *inst);

/* ── 线程控制（app 管理线程）──────────────────── */

void egw_transport_run(egw_transport_instance_t *inst);

void egw_transport_stop(egw_transport_instance_t *inst);

/* ── 连接操作（主线程调用 close，loop 线程调用 write）─ */

egw_err_t egw_transport_close(egw_transport_t *tp);

egw_err_t egw_transport_write(egw_transport_t *tp, const void *buf, size_t len);

#endif /* EGW_TRANSPORT_H */
