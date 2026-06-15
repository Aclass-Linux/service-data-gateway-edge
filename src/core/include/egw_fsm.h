/**
 * @file egw_fsm.h
 * @brief 通用状态机引擎（完全对齐 QP/C 数据类型）
 *
 * 支持 entry/exit 动作、返回值驱动的转移，预留 HSM 扩展。
 * 状态函数第一个参数约定为 ctx，EGW_TRAN/EGW_SUPER 宏通过
 * (egw_fsm_t *)ctx 访问 temp 字段，要求 egw_fsm_t 是 ctx 的
 * 嵌入结构的第一个成员。
 */

#ifndef EGW_FSM_H
#define EGW_FSM_H

#include <stdint.h>
#include <stdbool.h>

/* ── 框架保留信号（对齐 QSignal）───────────────────── */

#define EGW_EMPTY_SIG  ((uint16_t)0)  /* 空信号（预留） */
#define EGW_ENTRY_SIG  ((uint16_t)1)  /* 进入状态时自动投递 */
#define EGW_EXIT_SIG   ((uint16_t)2)  /* 离开状态时自动投递 */
#define EGW_INIT_SIG   ((uint16_t)3)  /* 初始转移（HSM 子状态选择） */
#define EGW_USER_SIG   ((uint16_t)4)  /* 用户信号从此开始 */

/* ── 事件（参考 QEvt 简化版）───────────────────────── */

typedef uint16_t egw_sig_t;

typedef struct {
    egw_sig_t   sig;
    void       *data;
} egw_event_t;

/* ── 前向声明 ──────────────────────────────────────── */

typedef struct egw_fsm egw_fsm_t;

/* ── 状态函数与返回值（对齐 QState / QStateHandler）──── */

typedef uint_fast8_t egw_state_t;
typedef egw_state_t (*egw_state_fn)(void *ctx, egw_event_t *ev);

#define EGW_RET_HANDLED   ((egw_state_t)2U)  /* 事件已处理 */
#define EGW_RET_TRAN      ((egw_state_t)3U)  /* 转移，目标在 temp */
#define EGW_RET_SUPER     ((egw_state_t)0U)  /* 未处理，冒泡父状态（预留 HSM） */
#define EGW_RET_IGNORED   ((egw_state_t)5U)  /* 忽略（预留） */

/* ── 状态机实例（对齐 QAsm 的 state + temp 模式）───── */

struct egw_fsm {
    egw_state_fn   current;   /* 当前状态 */
    egw_state_fn   temp;      /* EGW_TRAN/EGW_SUPER 暂存目标 */
};

#define EGW_TRAN(target_) \
    (((egw_fsm_t *)(ctx))->temp = (target_), EGW_RET_TRAN)

#define EGW_SUPER(super_) \
    (((egw_fsm_t *)(ctx))->temp = (super_), EGW_RET_SUPER)

/* ── 操作 ──────────────────────────────────────────── */

/**
 * @brief 初始化状态机
 *
 * 设置初始状态并自动执行该状态的 entry 动作。
 */
void egw_fsm_init(egw_fsm_t *fsm, egw_state_fn initial);

/**
 * @brief 派发事件到当前状态
 *
 * 调用当前状态函数。如果返回 EGW_RET_TRAN，则自动执行：
 *   exit(当前) → current = temp → entry(目标)
 */
void egw_fsm_dispatch(egw_fsm_t *fsm, egw_event_t *ev);

#endif /* EGW_FSM_H */
