#include "gateway_app.h"
#include "gateway_engine.h"
#include "config.h"
#include "egw_serial.h"
#include "egw_protocol.h"
#include "egw_persist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define MAX_PORTS     32
#define READ_BUF_SIZE 256

/* ── 事件信号 ───────────────────────────────────────── */

enum {
    EV_SIGINT      = EGW_USER_SIG,
    EV_TIMER_TICK,
};

/* ── 端口上下文 ─────────────────────────────────────── */

typedef struct {
    egw_serial_t        *tp;
    uv_poll_t            poll;
    egw_proto_ctx_t     *proto;
    egw_serial_params_t  params;
    uv_loop_t           *loop;
    egw_bus_t           *bus;
    uint8_t              read_buf[READ_BUF_SIZE];
} port_ctx_t;

/* ── 前向声明 ───────────────────────────────────────── */

static egw_state_t st_running(void *ctx, egw_event_t *ev);
static egw_state_t st_shutdown(void *ctx, egw_event_t *ev);
static void on_poll_cb(uv_poll_t *handle, int status, int events);
static void do_sched_poll(void);
static void do_poll_read(port_ctx_t *p);

/* ── 应用上下文 ─────────────────────────────────────── */

typedef struct {
    gw_engine_t     eng;          /* must be first (EGW_TRAN) */
    egw_conf_t     *cfg;
    egw_persist_t  *persist;
    port_ctx_t      ports[MAX_PORTS];
    int             port_count;
    int             cur_port;
    int             phase;
    uv_timer_t      sched_timer;
    uv_signal_t     sigint;
} app_t;

static app_t *g_app;

/* ── CRC-16 ──────────────────────────────────────────── */

static uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xA001u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* ── 调度 ────────────────────────────────────────────── */

static void do_sched_poll(void)
{
    app_t *app = g_app;

    if (app->port_count == 0) {
        app->phase = 0;
        return;
    }

    app->cur_port = (app->cur_port + 1) % app->port_count;
    port_ctx_t *p = &app->ports[app->cur_port];

    if (!p->tp) {
        app->phase = 0;
        return;
    }

    uint8_t req[8] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00 };
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFFu);
    req[7] = (uint8_t)(crc >> 8);

    egw_serial_write(p->tp, req, sizeof(req));
    egw_serial_flush(p->tp);

    app->phase = 1;

    int want = UV_READABLE;
    if (egw_serial_has_pending(p->tp)) {
        want |= UV_WRITABLE;
    }
    uv_poll_start(&p->poll, want, on_poll_cb);
}

/* ── 读响应 ──────────────────────────────────────────── */

static void do_poll_read(port_ctx_t *p)
{
    size_t    n   = 0;
    egw_err_t err;

    err = egw_serial_read(p->tp, p->read_buf, &n, READ_BUF_SIZE);
    if (err != EGW_OK) {
        egw_serial_close(p->tp);
        p->tp = NULL;
        uv_close((uv_handle_t *)&p->poll, NULL);
        {
            egw_serial_t *new_tp = NULL;
            err = egw_serial_open(&p->params, &new_tp);
            p->tp = (err == EGW_OK) ? new_tp : NULL;
        }
        if (p->tp) {
            uv_poll_init(p->loop, &p->poll,
                         egw_serial_get_fd(p->tp));
            uv_handle_set_data((uv_handle_t *)&p->poll, p);
            uv_poll_start(&p->poll, UV_READABLE, on_poll_cb);
        }
        g_app->phase = 0;
        return;
    }

    if (n == 0) {
        return;
    }

    egw_proto_result_t r = egw_proto_feed(p->proto, p->read_buf, n);
    if (r == EGW_PROTO_FRAME_READY) {
        size_t         frame_len = 0;
        const uint8_t *frame     = egw_proto_get_frame(p->proto, &frame_len);
        if (frame_len >= 3) {
            egw_value_t  val;
            val.raw = 0;
            if (frame_len >= 5) {
                val.i32 = (int32_t)(((uint32_t)frame[3] << 8) | frame[4]);
            }
            egw_bus_publish(p->bus, 0, 0, val);
        }
        egw_proto_reset(p->proto);
    } else if (r == EGW_PROTO_FRAME_ERROR) {
        /* auto-reset */
    }
}

/* ── 回调：poll ──────────────────────────────────────── */

static void on_poll_cb(uv_poll_t *handle, int status, int events)
{
    port_ctx_t *p = uv_handle_get_data((uv_handle_t *)handle);

    if (status < 0) {
        egw_serial_close(p->tp);
        p->tp = NULL;
        uv_close((uv_handle_t *)&p->poll, NULL);
        {
            egw_serial_t *new_tp = NULL;
            egw_err_t err = egw_serial_open(&p->params, &new_tp);
            p->tp = (err == EGW_OK) ? new_tp : NULL;
        }
        if (p->tp) {
            uv_poll_init(p->loop, &p->poll,
                         egw_serial_get_fd(p->tp));
            uv_handle_set_data((uv_handle_t *)&p->poll, p);
            uv_poll_start(&p->poll, UV_READABLE, on_poll_cb);
        }
        g_app->phase = 0;
        return;
    }

    if (events & UV_READABLE) {
        do_poll_read(p);
    }

    if (events & UV_WRITABLE) {
        egw_serial_flush(p->tp);
    }

    int want = UV_READABLE;
    if (egw_serial_has_pending(p->tp)) {
        want |= UV_WRITABLE;
    }
    uv_poll_start(&p->poll, want, on_poll_cb);
}

/* ── 回调：timer ─────────────────────────────────────── */

static void on_timer_cb(uv_timer_t *handle)
{
    (void)handle;
    app_t *app = g_app;

    if (app->phase == 0) {
        do_sched_poll();
    }
}

/* ── 回调：signal ────────────────────────────────────── */

static void on_sigint_cb(uv_signal_t *handle, int signum)
{
    (void)handle;
    (void)signum;
    app_t      *app = g_app;
    egw_event_t ev  = { .sig = EV_SIGINT, .data = NULL };

    egw_fsm_dispatch(&app->eng.fsm, &ev);
}

/* ── 状态 RUNNING ────────────────────────────────────── */

static egw_state_t st_running(void *ctx, egw_event_t *ev)
{
    switch (ev->sig) {
    case EGW_ENTRY_SIG:
    case EGW_EXIT_SIG:
        return EGW_RET_HANDLED;

    case EV_SIGINT:
        printf("\nShutting down...\n");
        return EGW_TRAN(st_shutdown);

    default:
        return EGW_RET_HANDLED;
    }
}

/* ── 状态 SHUTDOWN ──────────────────────────────────── */

static egw_state_t st_shutdown(void *ctx, egw_event_t *ev)
{
    app_t *app = (app_t *)ctx;

    switch (ev->sig) {
    case EGW_ENTRY_SIG:
        uv_stop(&app->eng.loop);
        return EGW_RET_HANDLED;

    default:
        return EGW_RET_HANDLED;
    }
}

/* ── 打开端口 ────────────────────────────────────────── */

static egw_err_t open_ports(app_t *app)
{
    int32_t n_ports;

    egw_conf_array_length(app->cfg, "/modbus/serial_ports", &n_ports, 0);

    for (int32_t p = 0; p < n_ports && app->port_count < MAX_PORTS; p++) {
        char path_key[128];
        char baud_key[128];
        char parity_key[128];

        snprintf(path_key,   sizeof(path_key),
                 "/modbus/serial_ports/%d/path", p);
        snprintf(baud_key,   sizeof(baud_key),
                 "/modbus/serial_ports/%d/baud", p);
        snprintf(parity_key, sizeof(parity_key),
                 "/modbus/serial_ports/%d/parity", p);

        char *path = NULL;
        egw_conf_get_string(app->cfg, path_key, &path, NULL);
        if (!path) continue;

        int32_t baud = 9600;
        egw_conf_get_int(app->cfg, baud_key, &baud, baud);

        char parity = 'N';
        char *ps = NULL;
        egw_conf_get_string(app->cfg, parity_key, &ps, NULL);
        if (ps) { parity = ps[0]; free(ps); }

        egw_serial_params_t sp = {
            .path = path, .baud = baud, .parity = parity,
            .data_bits = 8, .stop_bits = 1,
        };

        egw_serial_t *tp = NULL;
        egw_err_t err = egw_serial_open(&sp, &tp);
        if (err != EGW_OK) {
            printf("port[%d]: open failed (%d)\n", p, err);
            free(path);
            continue;
        }

        egw_proto_ctx_t *proto = egw_proto_ctx_create();
        if (!proto) { egw_serial_close(tp); free(path); continue; }

        port_ctx_t *pc = &app->ports[app->port_count];
        uv_poll_init(&app->eng.loop, &pc->poll, egw_serial_get_fd(tp));
        uv_handle_set_data((uv_handle_t *)&pc->poll, pc);
        uv_poll_start(&pc->poll, UV_READABLE, on_poll_cb);

        pc->tp      = tp;
        pc->proto   = proto;
        pc->params  = sp;
        pc->loop    = &app->eng.loop;
        pc->bus     = app->eng.bus;
        pc->params.path = strdup(path);
        free(path);
        app->port_count++;
        printf("  opened %s\n", sp.path);
    }

    return EGW_OK;
}

/* ── 总线回调（占位） ───────────────────────────────── */

static void on_bus_data(uint16_t device_id, uint32_t sig_id,
                        egw_value_t value, void *data)
{
    (void)data;
    printf("bus: dev=%u sig=%u val=%d\n",
           (unsigned)device_id, (unsigned)sig_id, value.i32);
}

/* ── 入口 ────────────────────────────────────────────── */

int egw_app_run(int argc, char *argv[])
{
    const char *cfg_path = "config.json";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cfg_path = argv[i + 1];
            i++;
        }
    }

    app_t app;
    memset(&app, 0, sizeof(app));
    g_app = &app;

    /* ── 引擎（loop + bus + fsm） ─────────────────── */

    egw_err_t err = gw_engine_init(&app.eng, st_running, &app);
    if (err != EGW_OK) {
        printf("Failed to initialize engine\n");
        return 1;
    }

    /* ── 配置 ────────────────────────────────────── */

    err = egw_conf_load(cfg_path, &app.cfg);
    if (err != EGW_OK) {
        printf("Failed to load config: %d\n", err);
        gw_engine_destroy(&app.eng);
        return 1;
    }

    /* ── 端口 ────────────────────────────────────── */

    printf("Opening serial ports...\n");
    open_ports(&app);
    if (app.port_count == 0) {
        printf("No ports opened, exiting.\n");
        egw_conf_free(app.cfg);
        gw_engine_destroy(&app.eng);
        return 1;
    }

    egw_bus_subscribe(app.eng.bus, 0xFFFF, 0xFFFFFFFF, on_bus_data, NULL);
    app.persist = egw_persist_create("gateway_persist.bin", 256);

    /* ── 定时器（采集调度） ──────────────────────── */

    uv_timer_init(&app.eng.loop, &app.sched_timer);
    uv_handle_set_data((uv_handle_t *)&app.sched_timer, &app);
    uv_timer_start(&app.sched_timer, on_timer_cb, 1000, 1000);

    /* ── SIGINT ──────────────────────────────────── */

    uv_signal_init(&app.eng.loop, &app.sigint);
    uv_handle_set_data((uv_handle_t *)&app.sigint, &app);
    uv_signal_start(&app.sigint, on_sigint_cb, SIGINT);

    /* ── 运行（引擎内部有 prepare → FSM tick） ─── */

    printf("Running (Ctrl+C to stop)...\n");
    gw_engine_run(&app.eng);

    /* ── 清理 ────────────────────────────────────── */

    egw_persist_destroy(app.persist);

    for (int i = 0; i < app.port_count; i++) {
        port_ctx_t *p = &app.ports[i];
        uv_poll_stop(&p->poll);
        uv_close((uv_handle_t *)&p->poll, NULL);
        egw_serial_close(p->tp);
        egw_proto_ctx_destroy(p->proto);
        free((void *)p->params.path);
    }

    egw_conf_free(app.cfg);
    gw_engine_destroy(&app.eng);
    printf("Goodbye.\n");
    return 0;
}
