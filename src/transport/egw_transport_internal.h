#ifndef EGW_TRANSPORT_INTERNAL_H
#define EGW_TRANSPORT_INTERNAL_H

#include "egw_transport.h"
#include <uv.h>
#include <pthread.h>

/* ── 前向声明 ──────────────────────────────────── */

struct egw_transport_instance;

/* ── vtable ────────────────────────────────────── */

struct egw_transport_ops {
    egw_err_t (*open)(struct egw_transport *tp);
    egw_err_t (*close)(struct egw_transport *tp);
    egw_err_t (*write)(struct egw_transport *tp, const void *buf, size_t len);
    void      (*destroy)(struct egw_transport *tp);
};

/* ── 基类（完整定义，内部使用）────────────────── */

struct egw_transport {
    const struct egw_transport_ops    *ops;
    struct egw_transport_instance     *inst;
    uint32_t                           id;
    uint32_t                           seq;
    egw_transport_cbs_t                cbs;
    bool                               opened;
};

/* ── 传输实例（内部定义）───────────────────────── */

struct egw_transport_instance {
    uv_loop_t          loop;
    uv_async_t         close_async;
    bool               running;

    /* 所有已注册的 transport（register → run 之间积累） */
    struct egw_transport **transports;
    size_t                 transport_cnt;
    size_t                 transport_cap;

    /* 跨线程关闭队列（主线程 push，loop 线程 pop） */
    struct egw_transport **close_queue;
    size_t                 close_cnt;
    size_t                 close_cap;
    pthread_mutex_t        close_lock;
};

/* ── 变种内部函数 ────────────────────────────── */

egw_err_t egw_serial_create(struct egw_transport **tp,
                             const egw_serial_params_t *params,
                             const egw_transport_cbs_t *cbs);

#endif /* EGW_TRANSPORT_INTERNAL_H */
