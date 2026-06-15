#include "egw_loop.h"
#include <uv.h>
#include <stdlib.h>
#include <string.h>

/* ── 内部结构定义 ──────────────────────────────────── */

struct egw_loop {
    uv_loop_t uv_loop;
    bool      should_stop;
};

/* ── 生命周期实现 ──────────────────────────────────── */

egw_loop_t *egw_loop_create(void) {
    egw_loop_t *loop = calloc(1, sizeof(*loop));
    if (!loop) {
        return NULL;
    }

    int ret = uv_loop_init(&loop->uv_loop);
    if (ret != 0) {
        free(loop);
        return NULL;
    }

    loop->should_stop = false;
    return loop;
}

egw_err_t egw_loop_run(egw_loop_t *loop) {
    if (!loop) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    /* 运行事件循环直到没有活动句柄或显式停止 */
    int ret = uv_run(&loop->uv_loop, UV_RUN_DEFAULT);

    if (ret != 0) {
        return EGW_ERR_LOOP_RUN;
    }

    return EGW_OK;
}

void egw_loop_stop(egw_loop_t *loop) {
    if (!loop) {
        return;
    }

    loop->should_stop = true;
    uv_stop(&loop->uv_loop);
}

static void close_handle_cb(uv_handle_t *handle, void *arg) {
    (void)arg;
    if (!uv_is_closing(handle)) {
        uv_close(handle, NULL);
    }
}

void egw_loop_destroy(egw_loop_t *loop) {
    if (!loop) {
        return;
    }

    /* 关闭所有未关闭的句柄 */
    uv_walk(&loop->uv_loop, close_handle_cb, NULL);

    /* 再运行一次循环以执行所有关闭回调 */
    uv_run(&loop->uv_loop, UV_RUN_DEFAULT);

    /* 关闭循环并释放资源 */
    int ret = uv_loop_close(&loop->uv_loop);
    if (ret == UV_EBUSY) {
        /* 如果还有未关闭的句柄，再尝试一次 */
        uv_run(&loop->uv_loop, UV_RUN_DEFAULT);
        uv_loop_close(&loop->uv_loop);
    }

    free(loop);
}

/* ── 内部访问接口 ──────────────────────────────────── */

void *egw_loop_get_uv_loop(egw_loop_t *loop) {
    return loop ? &loop->uv_loop : NULL;
}
