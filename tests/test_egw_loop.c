/* ── 测试 libuv 基本功能 ────────────────────────────── */

#include <stdio.h>
#include <signal.h>
#include <uv.h>

static uv_loop_t   g_loop;
static uv_timer_t  g_timer;

static void on_timer_cb(uv_timer_t *handle)
{
    (void)handle;
    static int count = 0;
    printf("Timer tick %d\n", ++count);

    if (count >= 3) {
        printf("Stopping loop after 3 ticks\n");
        uv_stop(&g_loop);
    }
}

static void on_sigint(int signum)
{
    (void)signum;
    printf("\nReceived SIGINT, stopping loop...\n");
    uv_stop(&g_loop);
}

static void close_all_cb(uv_handle_t *handle, void *arg)
{
    (void)arg;
    if (!uv_is_closing(handle)) {
        uv_close(handle, NULL);
    }
}

int main(void)
{
    printf("Testing event loop...\n");

    if (uv_loop_init(&g_loop) != 0) {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }
    printf("✓ Event loop created\n");

    signal(SIGINT, on_sigint);

    uv_timer_init(&g_loop, &g_timer);
    uv_timer_start(&g_timer, on_timer_cb, 1000, 1000);
    printf("✓ Timer started (1s interval, will stop after 3 ticks)\n");

    printf("Running event loop...\n");
    uv_run(&g_loop, UV_RUN_DEFAULT);
    printf("✓ Event loop exited normally\n");

    uv_walk(&g_loop, close_all_cb, NULL);
    uv_run(&g_loop, UV_RUN_DEFAULT);
    uv_loop_close(&g_loop);
    printf("✓ Event loop destroyed\n");

    printf("\nAll tests passed!\n");
    return 0;
}
