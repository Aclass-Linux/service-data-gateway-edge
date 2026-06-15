/* ── 测试 egw_loop 基本功能 ────────────────────────── */

#include "egw_loop.h"
#include <stdio.h>
#include <signal.h>
#include <uv.h>

static egw_loop_t *g_loop = NULL;

static void on_sigint(int signum) {
    (void)signum;
    printf("\nReceived SIGINT, stopping loop...\n");
    if (g_loop) {
        egw_loop_stop(g_loop);
    }
}

static void on_timer(uv_timer_t *handle) {
    static int count = 0;
    printf("Timer tick %d\n", ++count);

    if (count >= 3) {
        printf("Stopping loop after 3 ticks\n");
        egw_loop_stop(g_loop);
    }
}

int main(void) {
    printf("Testing egw_loop_t...\n");

    /* 创建事件循环 */
    g_loop = egw_loop_create();
    if (!g_loop) {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }
    printf("✓ Event loop created\n");

    /* 注册SIGINT处理器 */
    signal(SIGINT, on_sigint);

    /* 创建一个定时器验证事件循环工作 */
    uv_loop_t *uv_loop = egw_loop_get_uv_loop(g_loop);
    uv_timer_t timer;
    uv_timer_init(uv_loop, &timer);
    uv_timer_start(&timer, on_timer, 1000, 1000);  /* 每秒触发 */
    printf("✓ Timer started (1s interval, will stop after 3 ticks)\n");

    /* 运行事件循环 */
    printf("Running event loop...\n");
    egw_err_t err = egw_loop_run(g_loop);
    if (err != EGW_OK) {
        fprintf(stderr, "Event loop run failed: %d\n", err);
        egw_loop_destroy(g_loop);
        return 1;
    }
    printf("✓ Event loop exited normally\n");

    /* 销毁事件循环 */
    egw_loop_destroy(g_loop);
    printf("✓ Event loop destroyed\n");

    printf("\nAll tests passed!\n");
    return 0;
}
