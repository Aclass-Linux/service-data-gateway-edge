/**
 * @file egw_fsm.h
 * @brief 通用分层状态机引擎
 *
 * 纯工具，不涉及 I/O、事件循环或任何平台相关代码。
 * 状态通过函数指针表示，事件派发到当前状态函数。
 */

#ifndef EGW_FSM_H
#define EGW_FSM_H

#include <stdint.h>

/* ── 事件 ──────────────────────────────────────── */

typedef uint16_t egw_sig_t;

typedef struct {
    egw_sig_t   sig;
    void       *data;
} egw_event_t;

/* ── 状态函数 ──────────────────────────────────── */

typedef void (*egw_state_fn)(void *ctx, egw_event_t *ev);

/* ── 状态机实例（嵌入在调用者结构体中）───────────── */

typedef struct {
    egw_state_fn   current;
} egw_fsm_t;

/* ── 操作 ──────────────────────────────────────── */

static inline void egw_fsm_init(egw_fsm_t *fsm, egw_state_fn initial)
{
    fsm->current = initial;
}

static inline void egw_fsm_dispatch(egw_fsm_t *fsm, egw_event_t *ev)
{
    if (fsm->current) {
        fsm->current(fsm, ev);
    }
}

#define EGW_FSM_TRAN(fsm, target) \
    do { \
        (fsm)->current = (target); \
    } while (0)

#endif /* EGW_FSM_H */
