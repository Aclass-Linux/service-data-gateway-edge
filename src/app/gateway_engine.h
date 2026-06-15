/**
 * @file gateway_engine.h
 * @brief 应用层引擎 —— 组合 fsm + uv_loop + bus 的生命周期
 *
 * 提供 init/run/destroy 三步接口，内部自动在每轮 libuv 迭代前
 * 投递 EGW_ENGINE_TICK 到 FSM。
 */

#ifndef GW_ENGINE_H
#define GW_ENGINE_H

#include "egw_defs.h"
#include "egw_fsm.h"
#include "egw_bus.h"
#include <uv.h>

/* ── 引擎保留信号 ──────────────────────────────────── */

#define EGW_ENGINE_TICK  ((egw_sig_t)(EGW_USER_SIG + 16))

/* ── 引擎结构体 ─────────────────────────────────────── */

typedef struct {
    egw_fsm_t       fsm;         /* must be first (EGW_TRAN) */
    uv_loop_t       loop;
    egw_bus_t      *bus;
    void           *app_data;    /* 传给状态函数的 ctx */

    /* 内部句柄 */
    uv_prepare_t    _prepare;
} gw_engine_t;

/* ── API ────────────────────────────────────────────── */

egw_err_t gw_engine_init(gw_engine_t *eng, egw_state_fn init_state,
                          void *app_data);
void      gw_engine_run(gw_engine_t *eng);
void      gw_engine_destroy(gw_engine_t *eng);

#endif /* GW_ENGINE_H */
