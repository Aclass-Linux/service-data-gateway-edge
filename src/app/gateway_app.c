#include "gateway_app.h"
#include "config.h"
#include "egw_loop.h"
#include "egw_serial.h"
#include "egw_fsm.h"
#include "egw_protocol.h"
#include "egw_runtime.h"
#include "egw_bus.h"
#include "egw_persist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define MAX_PORTS     32
#define READ_BUF_SIZE 256

/* ── 事件信号 ───────────────────────────────────────── */

enum {
    EV_SIGINT,
    EV_TIMER_TICK,
};

/* ── 采集阶段 ────────────────────────────────────────── */

typedef enum {
    PHASE_SCHED_IDLE,
    PHASE_TX_SEND,
    PHASE_RX_WAITING,
    PHASE_PROC_DONE,
} app_phase_t;

/* ── 端口上下文 ─────────────────────────────────────── */

typedef struct {
    egw_serial_t        *tp;
    egw_poll_t           poll;
    egw_proto_ctx_t     *proto;
    int                  port_index;
    egw_serial_params_t  params;
    uint8_t              read_buf[READ_BUF_SIZE];
} port_ctx_t;

/* ── 应用上下文 ─────────────────────────────────────── */

typedef struct app_ctx {
    egw_fsm_t       fsm;
    egw_runtime_t  *rt;
    egw_conf_t     *cfg;
    egw_timer_t     sched_timer;
    egw_signal_t    sigint;
    egw_persist_t  *persist;
    port_ctx_t      ports[MAX_PORTS];
    int             port_count;
    int             cur_port;
    app_phase_t     phase;
} app_ctx_t;

/* ── 前向声明 ───────────────────────────────────────── */

static void st_running(void *fsm_ptr, egw_event_t *ev);
static void st_shutdown(void *fsm_ptr, egw_event_t *ev);
static void do_sched_poll(app_ctx_t *ctx);
static void do_poll_read(app_ctx_t *ctx, port_ctx_t *p);

/* ── 总线回调 ───────────────────────────────────────── */

static void on_bus_data(uint16_t device_id, uint32_t sig_id,
                        egw_value_t value, void *data)
{
    (void)data;
    printf("bus: dev=%u sig=%u val=%d\n",
           (unsigned)device_id, (unsigned)sig_id, value.i32);
}

/* ── 回调：poll ─────────────────────────────────────── */

static void on_poll_cb(egw_poll_t *poll, int status, int events, void *data)
{
    app_ctx_t  *ctx = data;
    port_ctx_t *p   = NULL;

    for (int i = 0; i < ctx->port_count; i++) {
        if (&ctx->ports[i].poll == poll) {
            p = &ctx->ports[i];
            break;
        }
    }
    if (!p) {
        return;
    }

    if (status < 0) {
        egw_serial_close(p->tp);
        p->tp = NULL;
        egw_poll_close(&p->poll);
        {
            egw_serial_t *new_tp = NULL;
            egw_err_t err = egw_serial_open(&p->params, &new_tp);
            p->tp = (err == EGW_OK) ? new_tp : NULL;
        }
        if (p->tp) {
            egw_loop_t *loop = egw_runtime_loop(ctx->rt);
            egw_poll_init(loop, &p->poll,
                          egw_serial_get_fd(p->tp));
            egw_poll_start(&p->poll, EGW_POLLIN, on_poll_cb, ctx);
        }
        return;
    }

    if (events & EGW_POLLIN) {
        do_poll_read(ctx, p);
    }

    if (events & EGW_POLLOUT) {
        egw_serial_flush(p->tp);
    }

    int want = EGW_POLLIN;
    if (egw_serial_has_pending(p->tp)) {
        want |= EGW_POLLOUT;
    }
    egw_poll_start(&p->poll, want, on_poll_cb, ctx);
}

/* ── 回调：timer ────────────────────────────────────── */

static void on_timer_cb(egw_timer_t *timer, void *data)
{
    (void)timer;
    app_ctx_t *ctx = data;

    if (ctx->phase == PHASE_SCHED_IDLE) {
        do_sched_poll(ctx);
    }
}

/* ── 回调：signal ───────────────────────────────────── */

static void on_sigint_cb(egw_signal_t *sig, int signum, void *data)
{
    (void)sig;
    (void)signum;
    app_ctx_t  *ctx = data;
    egw_event_t ev  = { .sig = EV_SIGINT, .data = NULL };

    egw_fsm_dispatch(&ctx->fsm, &ev);
}

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

static void do_sched_poll(app_ctx_t *ctx)
{
    if (ctx->port_count == 0) {
        ctx->phase = PHASE_SCHED_IDLE;
        return;
    }

    ctx->cur_port = (ctx->cur_port + 1) % ctx->port_count;
    port_ctx_t *p = &ctx->ports[ctx->cur_port];

    if (!p->tp) {
        ctx->phase = PHASE_SCHED_IDLE;
        return;
    }

    uint8_t req[8] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00 };
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFFu);
    req[7] = (uint8_t)(crc >> 8);

    egw_serial_write(p->tp, req, sizeof(req));
    egw_serial_flush(p->tp);

    ctx->phase = PHASE_RX_WAITING;

    int want = EGW_POLLIN;
    if (egw_serial_has_pending(p->tp)) {
        want |= EGW_POLLOUT;
    }
    egw_poll_start(&p->poll, want, on_poll_cb, ctx);
}

/* ── 读响应 ──────────────────────────────────────────── */

static void do_poll_read(app_ctx_t *ctx, port_ctx_t *p)
{
    size_t    n   = 0;
    egw_err_t err;

    err = egw_serial_read(p->tp, p->read_buf, &n, READ_BUF_SIZE);
    if (err != EGW_OK) {
        egw_serial_close(p->tp);
        p->tp = NULL;
        egw_poll_close(&p->poll);
        {
            egw_serial_t *new_tp = NULL;
            err = egw_serial_open(&p->params, &new_tp);
            p->tp = (err == EGW_OK) ? new_tp : NULL;
        }
        if (p->tp) {
            egw_loop_t *loop = egw_runtime_loop(ctx->rt);
            egw_poll_init(loop, &p->poll,
                          egw_serial_get_fd(p->tp));
            egw_poll_start(&p->poll, EGW_POLLIN, on_poll_cb, ctx);
        }
        ctx->phase = PHASE_SCHED_IDLE;
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
            egw_bus_t   *bus = egw_runtime_bus(ctx->rt);
            egw_value_t  val;
            val.raw = 0;
            if (frame_len >= 5) {
                val.i32 = (int32_t)(((uint32_t)frame[3] << 8) | frame[4]);
            }
            egw_bus_publish(bus, (uint16_t)ctx->cur_port,
                            (uint32_t)p->port_index, val);
            if (ctx->persist) {
                egw_persist_set(ctx->persist, (uint32_t)ctx->cur_port, val);
            }
        }
        egw_proto_reset(p->proto);
        ctx->phase = PHASE_SCHED_IDLE;
    } else if (r == EGW_PROTO_FRAME_ERROR) {
        ctx->phase = PHASE_SCHED_IDLE;
    }
}

/* ── 状态 RUNNING ────────────────────────────────────── */

static void st_running(void *fsm_ptr, egw_event_t *ev)
{
    app_ctx_t *ctx = (app_ctx_t *)fsm_ptr;

    switch (ev->sig) {
    case EV_SIGINT:
        printf("\nShutting down...\n");
        EGW_FSM_TRAN(&ctx->fsm, st_shutdown);
        st_shutdown(fsm_ptr, NULL);
        break;

    case EV_TIMER_TICK:
        do_sched_poll(ctx);
        break;

    default:
        break;
    }
}

/* ── 状态 SHUTDOWN ──────────────────────────────────── */

static void st_shutdown(void *fsm_ptr, egw_event_t *ev)
{
    (void)ev;
    app_ctx_t *ctx = (app_ctx_t *)fsm_ptr;

    egw_timer_stop(&ctx->sched_timer);
    egw_timer_close(&ctx->sched_timer);

    for (int i = 0; i < ctx->port_count; i++) {
        port_ctx_t *p = &ctx->ports[i];
        egw_poll_stop(&p->poll);
        egw_poll_close(&p->poll);
        egw_serial_close(p->tp);
        p->tp = NULL;
        egw_proto_ctx_destroy(p->proto);
        p->proto = NULL;
        free((void *)p->params.path);
        p->params.path = NULL;
    }

    egw_signal_close(&ctx->sigint);
    egw_loop_stop(egw_runtime_loop(ctx->rt));
}

/* ── 打开端口 ────────────────────────────────────────── */

static egw_err_t open_ports(app_ctx_t *ctx)
{
    egw_loop_t *loop = egw_runtime_loop(ctx->rt);
    int32_t n_ports;

    egw_conf_array_length(ctx->cfg, "/modbus/serial_ports",
                          &n_ports, 0);

    for (int32_t p = 0; p < n_ports && p < MAX_PORTS; p++) {
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
        egw_conf_get_string(ctx->cfg, path_key, &path, NULL);
        if (!path) {
            continue;
        }

        int32_t baud = 9600;
        egw_conf_get_int(ctx->cfg, baud_key, &baud, baud);

        char parity = 'N';
        char *ps = NULL;
        egw_conf_get_string(ctx->cfg, parity_key, &ps, NULL);
        if (ps) {
            parity = ps[0];
            free(ps);
        }

        egw_serial_params_t sp = {
            .path      = path,
            .baud      = baud,
            .parity    = parity,
            .data_bits = 8,
            .stop_bits = 1,
        };

        egw_serial_t *tp  = NULL;
        egw_err_t     err = egw_serial_open(&sp, &tp);
        if (err != EGW_OK) {
            printf("port[%d]: open failed (%d)\n", p, err);
            free(path);
            continue;
        }

        port_ctx_t *pc = &ctx->ports[ctx->port_count];
        pc->proto = egw_proto_ctx_create();
        if (!pc->proto) {
            egw_serial_close(tp);
            free(path);
            continue;
        }

        egw_poll_init(loop, &pc->poll, egw_serial_get_fd(tp));
        egw_poll_start(&pc->poll, EGW_POLLIN, on_poll_cb, ctx);

        pc->tp         = tp;
        pc->port_index = ctx->port_count;
        pc->params     = sp;
        pc->params.path = strdup(path);
        free(path);
        ctx->port_count++;
        printf("  opened %s\n", sp.path);
    }

    return EGW_OK;
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

    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    egw_fsm_init(&ctx.fsm, st_running);

    /* 创建事件循环 */
    egw_loop_t *loop = egw_loop_create();
    if (!loop) {
        printf("Failed to create event loop\n");
        return 1;
    }

    /* 创建总线 */
    egw_bus_t *bus = egw_bus_create();
    if (!bus) {
        egw_loop_destroy(loop);
        return 1;
    }

    /* 创建运行时 */
    ctx.rt = egw_runtime_create(loop, bus);
    if (!ctx.rt) {
        egw_bus_destroy(bus);
        egw_loop_destroy(loop);
        return 1;
    }

    /* 订阅总线数据 */
    egw_bus_subscribe(bus, 0xFFFF, 0xFFFFFFFF, on_bus_data, NULL);

    /* 加载配置 */
    egw_err_t err = egw_conf_load(cfg_path, &ctx.cfg);
    if (err != EGW_OK) {
        printf("Failed to load config: %d\n", err);
        egw_runtime_destroy(ctx.rt);
        egw_bus_destroy(bus);
        egw_loop_destroy(loop);
        return 1;
    }

    /* 初始化持久化（可选，文件不存在时静默跳过） */
    ctx.persist = egw_persist_create("gateway_persist.bin", 256);

    /* 打开端口 */
    printf("Opening serial ports...\n");
    open_ports(&ctx);
    if (ctx.port_count == 0) {
        printf("No ports opened, exiting.\n");
        egw_persist_destroy(ctx.persist);
        egw_conf_free(ctx.cfg);
        egw_runtime_destroy(ctx.rt);
        egw_bus_destroy(bus);
        egw_loop_destroy(loop);
        return 1;
    }

    /* 采集定时器 */
    egw_timer_init(loop, &ctx.sched_timer);
    egw_timer_start(&ctx.sched_timer, 1000, 1000, on_timer_cb, &ctx);

    /* SIGINT */
    egw_signal_init(loop, &ctx.sigint, SIGINT, on_sigint_cb, &ctx);

    ctx.phase = PHASE_SCHED_IDLE;

    printf("Running (Ctrl+C to stop)...\n");
    err = egw_loop_run(loop);
    if (err != EGW_OK) {
        printf("Event loop error: %d\n", err);
    }

    egw_persist_destroy(ctx.persist);
    egw_conf_free(ctx.cfg);
    egw_runtime_destroy(ctx.rt);
    egw_bus_destroy(bus);
    egw_loop_destroy(loop);
    printf("Goodbye.\n");
    return 0;
}
