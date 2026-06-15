#include "gateway_engine.h"
#include <stdlib.h>
#include <string.h>

/* ── prepare 回调：每轮迭代前投递 TICK 到 FSM ───────── */

static void on_prepare_cb(uv_prepare_t *handle)
{
    gw_engine_t  *eng = (gw_engine_t *)handle->data;
    egw_event_t   tick = { .sig = EGW_ENGINE_TICK, .data = NULL };

    egw_fsm_dispatch(&eng->fsm, &tick);
}

/* ── uv_walk 回调：关闭所有未关闭句柄 ──────────────── */

static void close_all_cb(uv_handle_t *handle, void *arg)
{
    (void)arg;
    if (!uv_is_closing(handle)) {
        uv_close(handle, NULL);
    }
}

/* ── 生命周期 ────────────────────────────────────────── */

egw_err_t gw_engine_init(gw_engine_t *eng, egw_state_fn init_state,
                          void *app_data)
{
    if (!eng) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    if (uv_loop_init(&eng->loop) != 0) {
        eng->bus = NULL;
        return EGW_RET_CODE(ERR_NOMEM);
    }

    eng->bus = egw_bus_create();
    if (!eng->bus) {
        uv_loop_close(&eng->loop);
        return EGW_RET_CODE(ERR_NOMEM);
    }

    eng->app_data = app_data;

    if (init_state) {
        egw_fsm_init(&eng->fsm, init_state);
    }

    return EGW_OK;
}

void gw_engine_run(gw_engine_t *eng)
{
    if (!eng || !eng->fsm.current) {
        return;
    }

    /* prepare handle：每轮迭代前触发 */
    uv_prepare_init(&eng->loop, &eng->_prepare);
    eng->_prepare.data = eng;
    uv_prepare_start(&eng->_prepare, on_prepare_cb);

    uv_run(&eng->loop, UV_RUN_DEFAULT);

    uv_close((uv_handle_t *)&eng->_prepare, NULL);
    uv_run(&eng->loop, UV_RUN_DEFAULT);
}

void gw_engine_destroy(gw_engine_t *eng)
{
    if (!eng) {
        return;
    }

    if (eng->bus) {
        egw_bus_destroy(eng->bus);
        eng->bus = NULL;
    }

    {   /* 关闭所有 uv 句柄 */
        struct close_walk_ctx { int dummy; };
        uv_walk(&eng->loop, &close_all_cb, NULL);
        uv_run(&eng->loop, UV_RUN_DEFAULT);
    }

    uv_loop_close(&eng->loop);
}
