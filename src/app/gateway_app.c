#include "gateway_app.h"
#include "config.h"
#include "egw_serial.h"
#include "egw_protocol.h"
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#define MAX_PORTS  32
#define READ_BUF   256

typedef struct {
    egw_serial_t        *tp;
    uv_poll_t            poll;
    int                  id;
    egw_serial_params_t  params;
    uint8_t              read_buf[READ_BUF];
} port_ctx_t;

static port_ctx_t       ports[MAX_PORTS];
static int              port_count;
static uv_loop_t       *g_loop;
static egw_conf_t      *g_cfg;

static void on_fd_ready(uv_poll_t *handle, int status, int events);

/* ── port lifecycle ────────────────────────────── */

static void on_poll_closed(uv_handle_t *handle)
{
    port_ctx_t *ctx = (port_ctx_t *)handle->data;

    egw_serial_close(ctx->tp);
    ctx->tp = NULL;
    free((void *)ctx->params.path);
    ctx->params.path = NULL;
}

static void close_port(port_ctx_t *ctx)
{
    uv_poll_stop(&ctx->poll);
    uv_close((uv_handle_t *)&ctx->poll, on_poll_closed);
}

static void reopen_port(port_ctx_t *ctx)
{
    egw_serial_t *new_tp = NULL;
    egw_err_t err = egw_serial_open(&ctx->params, &new_tp);
    if (err != EGW_OK) {
        fprintf(stderr, "reopen port failed: %d\n", err);
        return;
    }

    ctx->tp = new_tp;
    uv_poll_init(g_loop, &ctx->poll, egw_serial_get_fd(new_tp));
    ctx->poll.data = ctx;
    uv_poll_start(&ctx->poll, POLLIN, on_fd_ready);
    printf("  reopen ok\n");
}

/* ── fd event callback ────────────────────────── */

static void on_fd_ready(uv_poll_t *handle, int status, int events)
{
    port_ctx_t *ctx = (port_ctx_t *)handle->data;

    if (status < 0) {
        close_port(ctx);
        reopen_port(ctx);
        return;
    }

    if (events & POLLIN)
    {
        size_t len = 0;
        egw_err_t err = egw_serial_read(ctx->tp, ctx->read_buf,
                                         &len, READ_BUF);
        if (err != EGW_OK) {
            fprintf(stderr, "port %d read error\n", ctx->id);
            close_port(ctx);
            reopen_port(ctx);
            return;
        }

        if (len > 0) {
            egw_protocol_process((uint32_t)ctx->id,
                                  ctx->read_buf, len);
        }

        egw_serial_flush(ctx->tp);
    }

    if (events & POLLOUT)
    {
        egw_serial_flush(ctx->tp);
    }

    int want = POLLIN;
    if (egw_serial_has_pending(ctx->tp)) {
        want |= POLLOUT;
    }
    uv_poll_start(&ctx->poll, want, on_fd_ready);
}

/* ── signal ────────────────────────────────────── */

static void on_sigint(uv_signal_t *handle, int signum)
{
    (void)signum;
    printf("\nStopping...\n");
    uv_stop(handle->loop);
}

/* ── open ports from config ────────────────────── */

static egw_err_t open_ports(void)
{
    int32_t n_ports;
    egw_conf_array_length(g_cfg, "/modbus/serial_ports", &n_ports, 0);

    for (int32_t p = 0; p < n_ports && p < MAX_PORTS; p++)
    {
        char base[64];
        snprintf(base, sizeof(base), "/modbus/serial_ports/%d", p);

        char *path = NULL;
        egw_conf_get_string(g_cfg, base, &path, NULL);
        if (!path) {
            fprintf(stderr, "port[%d]: missing path\n", p);
            continue;
        }

        int32_t baud = 9600;
        egw_conf_get_int(g_cfg, base, &baud, baud);

        char parity = 'N';
        char *parity_str = NULL;
        egw_conf_get_string(g_cfg, base, &parity_str, NULL);
        if (parity_str) {
            parity = parity_str[0];
            free(parity_str);
        }

        egw_serial_params_t sp = {
            .path      = path,
            .baud      = baud,
            .parity    = parity,
            .data_bits = 8,
            .stop_bits = 1,
        };

        egw_serial_t *tp = NULL;
        egw_err_t err = egw_serial_open(&sp, &tp);
        if (err != EGW_OK) {
            fprintf(stderr, "port[%d]: open failed: %d\n", p, err);
            free(path);
            continue;
        }

        port_ctx_t *ctx = &ports[port_count];

        uv_poll_init(g_loop, &ctx->poll, egw_serial_get_fd(tp));
        ctx->poll.data = ctx;
        uv_poll_start(&ctx->poll, POLLIN, on_fd_ready);

        ctx->tp = tp;
        ctx->id = port_count;
        ctx->params = sp;
        ctx->params.path = strdup(path);
        free(path);
        port_count++;

        printf("  opened port %s\n", sp.path);
    }

    return EGW_OK;
}

/* ── app entry ─────────────────────────────────── */

int egw_app_run(int argc, char *argv[])
{
    const char *cfg_path = "config.json";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cfg_path = argv[i + 1];
            i++;
        }
    }

    uv_loop_t loop;
    uv_loop_init(&loop);
    g_loop = &loop;

    egw_err_t err = egw_conf_load(cfg_path, &g_cfg);
    if (err != EGW_OK) {
        fprintf(stderr, "Failed to load config: %d\n", err);
        uv_loop_close(&loop);
        return 1;
    }

    printf("Opening serial ports...\n");
    open_ports();

    uv_signal_t sigint;
    uv_signal_init(&loop, &sigint);
    uv_signal_start(&sigint, on_sigint, SIGINT);

    printf("Running event loop (Ctrl+C to stop)...\n");
    uv_run(&loop, UV_RUN_DEFAULT);

    uv_close((uv_handle_t *)&sigint, NULL);
    uv_run(&loop, UV_RUN_NOWAIT);

    for (int i = 0; i < port_count; i++) {
        uv_poll_stop(&ports[i].poll);
        uv_close((uv_handle_t *)&ports[i].poll, on_poll_closed);
    }
    uv_run(&loop, UV_RUN_DEFAULT);

    egw_conf_free(g_cfg);
    uv_loop_close(&loop);
    printf("Goodbye.\n");
    return 0;
}
