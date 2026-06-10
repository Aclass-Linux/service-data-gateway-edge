/**
 * @file test_uv_helper.h
 * @brief 异步测试辅助 —— 在 libuv event loop 中运行指定毫秒后停止
 *
 * 用法：
 *   test_uv_run_for(loop, 100);  // 跑 loop 100ms
 */

#ifndef TEST_UV_HELPER_H
#define TEST_UV_HELPER_H

#include <uv.h>

static void test_uv_on_timer_done(uv_timer_t *timer) {
    (void)timer;
}

static inline void test_uv_run_for(uv_loop_t *loop, int ms) {
    uv_timer_t timer;
    uv_timer_init(loop, &timer);
    uv_timer_start(&timer, test_uv_on_timer_done, (uint64_t)ms, 0);
    uv_run(loop, UV_RUN_DEFAULT);
    uv_close((uv_handle_t *)&timer, NULL);
    uv_run(loop, UV_RUN_DEFAULT);
}

#endif /* TEST_UV_HELPER_H */