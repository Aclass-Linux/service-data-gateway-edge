/**
 * @file egw_loop.h
 * @brief Event loop abstraction over libuv
 *
 * Core 层封装 libuv 事件循环，成为事件循环的唯一所有者。
 * 提供 fd 就绪通知（poll）、定时器（timer）、信号（signal）API。
 * 其他层通过此 API 访问事件循环，不直接调用 libuv。
 *
 * 参考：ADR-0006 Core层持有事件循环
 */

#ifndef EGW_LOOP_H
#define EGW_LOOP_H

#include "egw_defs.h"
#include <uv.h>

/* ── 不透明句柄 ────────────────────────────────────── */

typedef struct egw_loop   egw_loop_t;
typedef struct egw_poll   egw_poll_t;
typedef struct egw_timer  egw_timer_t;
typedef struct egw_signal egw_signal_t;

/* ── 回调类型 ──────────────────────────────────────── */

typedef void (*egw_poll_cb)  (egw_poll_t *poll, int status, int events, void *data);
typedef void (*egw_timer_cb) (egw_timer_t *timer, void *data);
typedef void (*egw_signal_cb)(egw_signal_t *sig, int signum, void *data);

/* ── 结构体定义（调用者可嵌入）────────────────────────── */

struct egw_loop {
    uv_loop_t uv_loop;
    bool      should_stop;
};

struct egw_poll {
    uv_poll_t   handle;
    egw_poll_cb cb;
    void       *data;
};

struct egw_timer {
    uv_timer_t  handle;
    egw_timer_cb cb;
    void        *data;
};

struct egw_signal {
    uv_signal_t   handle;
    egw_signal_cb cb;
    void         *data;
};

/* ── 事件循环生命周期 ──────────────────────────────── */

egw_loop_t *egw_loop_create(void);
egw_err_t   egw_loop_run(egw_loop_t *loop);
void        egw_loop_stop(egw_loop_t *loop);
void        egw_loop_destroy(egw_loop_t *loop);

/* ── fd 就绪通知（poll）────────────────────────────── */

#define EGW_POLLIN  0x01
#define EGW_POLLOUT 0x02

egw_err_t egw_poll_init(egw_loop_t *loop, egw_poll_t *poll, int fd);
egw_err_t egw_poll_start(egw_poll_t *poll, int events, egw_poll_cb cb, void *data);
egw_err_t egw_poll_stop(egw_poll_t *poll);
void      egw_poll_close(egw_poll_t *poll);

/* ── 定时器 ────────────────────────────────────────── */

egw_err_t egw_timer_init(egw_loop_t *loop, egw_timer_t *timer);
egw_err_t egw_timer_start(egw_timer_t *timer, uint64_t timeout_ms,
                           uint64_t repeat_ms, egw_timer_cb cb, void *data);
egw_err_t egw_timer_stop(egw_timer_t *timer);
void      egw_timer_close(egw_timer_t *timer);

/* ── 信号 ──────────────────────────────────────────── */

egw_err_t egw_signal_init(egw_loop_t *loop, egw_signal_t *sig,
                           int signum, egw_signal_cb cb, void *data);
void      egw_signal_close(egw_signal_t *sig);

/* ── 内部访问接口（仅供 Core 层内部 / 过渡期使用）───── */

void *egw_loop_get_uv_loop(egw_loop_t *loop);

#endif /* EGW_LOOP_H */
