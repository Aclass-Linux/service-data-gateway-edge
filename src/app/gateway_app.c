#include "gateway_app.h"
#include "egw_ptable.h"
#include "egw_modbus_core.h"
#include "egw_modbus_client.h"
#include "egw_modbus_server.h"
#include "egw_transport.h"
#include <string.h>
#include <stdlib.h>
#include <uv.h>

/* ── 点表配置结构体（协议层只推原始寄存器，语义在 app 层） ─ */

#define EGW_MODBUS_MASTER_ENABLED           (1u << 0)
#define EGW_MODBUS_MASTER_HAS_SCALE_OFFSET  (1u << 1)
#define EGW_MODBUS_MASTER_HAS_DEADBAND      (1u << 2)

typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
    uint8_t  func_code;
    uint16_t reg_addr;
    uint16_t reg_count;
    uint8_t  ctype;
    uint32_t poll_interval_ms;
    uint8_t  flags;
    float    scale;
    float    offset;
    float    deadband;
} egw_modbus_master_t;

#define EGW_MODBUS_SLAVE_ENABLED           (1u << 0)
#define EGW_MODBUS_SLAVE_HAS_SCALE_OFFSET  (1u << 1)
#define EGW_MODBUS_SLAVE_HAS_DEADBAND      (1u << 2)

typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
    uint8_t  func_code;
    uint16_t reg_addr;
    uint8_t  ctype;
    uint8_t  flags;
    float    scale;
    float    offset;
    float    deadband;
} egw_modbus_slave_t;

static const egw_field_t s_master_fields[] = {
    EGW_FIELD(egw_modbus_master_t, "device_id",        device_id,        EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_master_t, "sig_id",           sig_id,           EGW_CTYPE_U32),
    EGW_FIELD(egw_modbus_master_t, "func_code",        func_code,        EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_master_t, "reg_addr",         reg_addr,         EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_master_t, "reg_count",        reg_count,        EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_master_t, "ctype",            ctype,            EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_master_t, "poll_interval_ms", poll_interval_ms, EGW_CTYPE_U32),
    EGW_FIELD(egw_modbus_master_t, "flags",            flags,            EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_master_t, "scale",            scale,            EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_master_t, "offset",           offset,           EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_master_t, "deadband",         deadband,         EGW_CTYPE_F32),
};

static const egw_field_t s_slave_fields[] = {
    EGW_FIELD(egw_modbus_slave_t, "device_id", device_id, EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_slave_t, "sig_id",    sig_id,    EGW_CTYPE_U32),
    EGW_FIELD(egw_modbus_slave_t, "func_code", func_code, EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "reg_addr",  reg_addr,  EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_slave_t, "ctype",     ctype,     EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "flags",     flags,     EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "scale",     scale,     EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_slave_t, "offset",    offset,    EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_slave_t, "deadband",  deadband,  EGW_CTYPE_F32),
};

static const egw_field_t *egw_modbus_master_fields(size_t *count)
{
    if (count) { *count = sizeof(s_master_fields) / sizeof(s_master_fields[0]); }
    return s_master_fields;
}

static const egw_field_t *egw_modbus_slave_fields(size_t *count)
{
    if (count) { *count = sizeof(s_slave_fields) / sizeof(s_slave_fields[0]); }
    return s_slave_fields;
}

static void register_table(egw_ptable_t *pt, const char *name)
{
    const egw_field_t *flds = NULL;
    size_t nf = 0;
    size_t row_sz = 0;

    if (strcmp(name, "southbound") == 0) {
        flds = egw_modbus_master_fields(&nf);
        row_sz = sizeof(egw_modbus_master_t);
    } else if (strcmp(name, "northbound") == 0) {
        flds = egw_modbus_slave_fields(&nf);
        row_sz = sizeof(egw_modbus_slave_t);
    } else if (strcmp(name, "route") == 0) {
        flds = egw_ptable_route_fields(&nf);
        row_sz = sizeof(egw_route_entry_t);
    } else { return; }

    egw_buf_t r = egw_ptable_register(pt, name,
                    (egw_buf_t){ .data = (void *)flds, .len = nf * sizeof(egw_field_t) },
                    row_sz);
    if (!r.data) { return; }

    int nrow = r.len / row_sz;
    for (int i = 0; i < nrow; i++) {
        uint8_t *row = (uint8_t *)r.data + i * row_sz;

        if (strcmp(name, "southbound") == 0) {
            egw_modbus_master_t *p = (egw_modbus_master_t *)row;
            EGW_LOGI("  reg south dev=%u sig=%u fc=%u addr=%u count=%u ctype=%u",
                     p->device_id, p->sig_id, p->func_code,
                     p->reg_addr, p->reg_count, p->ctype);
        } else if (strcmp(name, "northbound") == 0) {
            egw_modbus_slave_t *p = (egw_modbus_slave_t *)row;
            EGW_LOGI("  reg north dev=%u sig=%u fc=%u addr=%u ctype=%u",
                     p->device_id, p->sig_id, p->func_code,
                     p->reg_addr, p->ctype);
        } else if (strcmp(name, "route") == 0) {
            egw_route_entry_t *p = (egw_route_entry_t *)row;
            EGW_LOGI("  reg route dev=%u sig=%u ctype=%u",
                     p->device_id, p->sig_id, p->ctype);
        }
    }
    EGW_LOGI("  register %s: loaded %d row(s)", name, nrow);
    free(r.data);
}

static void on_protocol_node(egw_ptable_t *pt, egw_node_t *n)
{
    const char *manifest = n->desc;
    EGW_LOGI("  protocol = %s", manifest);
    if (!manifest || !manifest[0]) { return; }

    egw_manifest_t *mh = egw_ptable_discover(pt, manifest);
    if (!mh) { return; }

    uint32_t nt = egw_manifest_count(mh);
    EGW_LOGI("  discovered %u table(s) from %s:", nt, manifest);
    for (uint32_t i = 0; i < nt; i++) {
        const egw_ptable_tbl_t *t = egw_manifest_get(mh, i);
        EGW_LOGI("    [%u] %s  (%s)", i, t->name, t->protocol);
        register_table(pt, t->name);
    }

    egw_manifest_free(mh);
}

static void on_port_node(egw_node_t *n)
{
    const char *path = n->desc[0] ? n->desc : "(empty)";
    EGW_LOGI("  port = %s", path);
}

/* ── Modbus RTU 本地回环（uv_poll 驱动） ─────────────── */
/*
 * 数据流（单进程，虚拟串口对 socat /tmp/ttyV0 ↔ /tmp/ttyV1）：
 *
 *   Client (主站)                          Server (从站)
 *   /tmp/ttyV0                             /tmp/ttyV1
 *       │                                      │
 *   1.build req → write ─────────────────→ 2.uv_poll readable
 *       │                                      │
 *       │                                  3.reserve + read + commit
 *       │                                  4.read_cb → build resp → write
 *       │ ←─────────────────────────────────     │
 *   5.uv_poll readable                          │
 *   6.reserve + read + commit                   │
 *   7.req_process → done_cb → uv_stop           │
 *
 * 全程零拷贝：transport read 直接写入 protocol 的 reserve 缓冲区。
 */

typedef enum {
    PHASE_SERVER_RECV,   /* 等从站收请求 */
    PHASE_CLIENT_RECV,   /* 等主站收响应 */
    PHASE_DONE,
} loopback_phase_t;

typedef struct {
    uv_loop_t           *loop;
    egw_transport_handle_t *cli_h;
    egw_transport_handle_t *srv_h;
    egw_modbus_server_t *server;
    egw_modbus_client_t    *cli;
    loopback_phase_t     phase;
    bool                 seg_pending;   /* 后半段待发送 */
    uv_poll_t            cli_poll;
    uv_poll_t            srv_poll;
} loopback_ctx_t;

static egw_err_t loopback_srv_read_cb(uint16_t addr, uint16_t qty,
                                        uint16_t *regs,
                                        uint8_t unit_id, void *arg)
{
    (void)unit_id; (void)arg;
    EGW_LOGI("    [server] read_cb: addr=%u qty=%u", addr, qty);
    for (uint16_t i = 0; i < qty; i++) {
        regs[i] = 0x000A + addr + i;
    }
    return EGW_OK;
}

static void loopback_cli_done_cb(const egw_modbus_result_t *result, void *arg)
{
    loopback_ctx_t *lb = arg;
    if (result->reg_count < 0) {
        EGW_LOGE("    [client] done_cb: error unit=%u addr=%u",
                 result->unit_id, result->addr);
        uv_stop(lb->loop);
        return;
    }
    EGW_LOGI("    [client] done_cb: unit=%u addr=%u regs[0]=0x%04X regs[1]=0x%04X",
             result->unit_id, result->addr,
             result->regs[0], result->reg_count > 1 ? result->regs[1] : 0);
    lb->phase = PHASE_DONE;
    uv_stop(lb->loop);
}

/* ── 前置声明 ───────────────────────────────────────── */

static void on_cli_poll(uv_poll_t *p, int status, int events);

/* ── server fd 可读回调 ─────────────────────────────── */

static void on_srv_poll(uv_poll_t *p, int status, int events)
{
    loopback_ctx_t *lb = p->data;
    if (status < 0 || !(events & UV_READABLE)) {
        return;
    }

    /* 零拷贝：transport 直接读入 server 的 protocol 缓冲区 */
    size_t avail = 0;
    uint8_t *wp = egw_modbus_server_reserve(lb->server, &avail);
    if (!wp) {
        return;
    }

    size_t rlen = 0;
    if (egw_transport_read(lb->srv_h, wp, &rlen, avail) != EGW_OK || rlen == 0) {
        return;
    }

    egw_modbus_server_commit(lb->server, rlen);

    size_t resp_len = 0;
    const uint8_t *resp = egw_modbus_server_get_response(lb->server, &resp_len);
    if (!resp) {
        return;
    }
    EGW_LOGI("    [server] recv request, → response ready");

    /* 模拟分段发送：先发前半段，后半段由 client 首轮 poll 负责 */
    size_t first_half = resp_len / 2;
    size_t written = 0;
    egw_transport_write(lb->srv_h, resp, &written, first_half);
    EGW_LOGI("    [server] → sent %zu/%zu bytes (first half)", written, resp_len);

    uv_poll_stop(&lb->srv_poll);
    lb->seg_pending = true;
    uv_poll_start(&lb->cli_poll, UV_READABLE, on_cli_poll);
}

/* ── client fd 可读回调 ─────────────────────────────── */

static void on_cli_poll(uv_poll_t *p, int status, int events)
{
    loopback_ctx_t *lb = p->data;
    if (status < 0 || !(events & UV_READABLE)) {
        return;
    }

    /* 零拷贝：transport 直接读入 client 的接收缓冲区 */
    size_t avail = 0;
    uint8_t *wp = egw_modbus_client_reserve(lb->cli, &avail);
    if (!wp) {
        return;
    }

    size_t rlen = 0;
    if (egw_transport_read(lb->cli_h, wp, &rlen, avail) != EGW_OK || rlen == 0) {
        return;
    }

    egw_modbus_client_commit(lb->cli, rlen);

    /* 如果还有后半段未发，现在发 */
    if (lb->seg_pending) {
        lb->seg_pending = false;
        size_t resp_len = 0;
        const uint8_t *resp = egw_modbus_server_get_response(lb->server, &resp_len);
        if (resp) {
            size_t first_half = resp_len / 2;
            size_t written = 0;
            egw_transport_write(lb->srv_h, resp + first_half,
                                &written, resp_len - first_half);
            EGW_LOGI("    [server] → sent %zu/%zu bytes (second half)",
                     written, resp_len - first_half);
            egw_modbus_server_response_sent(lb->server);
        }
    }
}

/* ── 超时定时器 ─────────────────────────────────────── */

static void on_timeout(uv_timer_t *t)
{
    loopback_ctx_t *lb = t->data;
    EGW_LOGE("  loopback timeout (phase=%d)", lb->phase);
    uv_stop(lb->loop);
}

/* ── 回环主函数 ─────────────────────────────────────── */

static void run_modbus_loopback(void)
{
    EGW_LOGI("=== Modbus RTU local loopback (uv_poll) ===");
    EGW_LOGI("  (requires: ./tools/virtual_serial.sh start)");

    loopback_ctx_t lb = {0};
    uv_loop_t      loop;
    uv_timer_t     timer;

    if (uv_loop_init(&loop) != 0) {
        EGW_LOGE("  uv_loop_init failed");
        return;
    }
    lb.loop = &loop;

    /* 1. 打开虚拟串口对的两端 */
    egw_transport_serial_params_t sp = {
        .baud = 9600, .parity = 'N', .data_bits = 8, .stop_bits = 1,
    };

    sp.path = "/tmp/ttyV0";
    lb.cli_h = egw_transport_serial_open(&sp);
    sp.path = "/tmp/ttyV1";
    lb.srv_h = egw_transport_serial_open(&sp);

    if (!lb.cli_h || !lb.srv_h) {
        EGW_LOGE("  open failed — run ./tools/virtual_serial.sh start");
        goto cleanup;
    }
    EGW_LOGI("  serial: /tmp/ttyV0 (client) + /tmp/ttyV1 (server) opened");

    /* 2. 创建从站 (RTU, unit=1) */
    lb.server = egw_modbus_server_create(&(egw_modbus_server_params_t){
        .transport = EGW_MODBUS_RTU,
        .unit_ids  = {1},
        .read_cb   = loopback_srv_read_cb,
    });
    if (!lb.server) {
        EGW_LOGE("  server_create failed");
        goto cleanup;
    }

    /* 3. 创建主站 + 注册请求 (FC03 read 2 holding regs, addr=0, unit=1) */
    lb.cli = egw_modbus_client_create(&(egw_modbus_client_params_t){
        .transport = EGW_MODBUS_RTU,
        .done_cb   = loopback_cli_done_cb,
        .cb_arg    = &lb,
    });
    if (!lb.cli) {
        EGW_LOGE("  client_create failed");
        goto cleanup;
    }
    egw_modbus_req_slot_t *req_slot = egw_modbus_client_register(lb.cli,
        &(egw_modbus_encode_params_t){
            .unit_id  = 1,
            .funccode = EGW_MODBUS_FC_READ_HOLDING_REGISTERS,
            .addr     = 0,
            .count    = 2,
        });
    if (!req_slot) {
        EGW_LOGE("  client register failed");
        goto cleanup;
    }
    EGW_LOGI("  client: req built (fc=03 addr=0 qty=2)");

    /* 4. Client 发送请求 → /tmp/ttyV0 */
    {
        size_t frame_len = 0;
        const uint8_t *frame = egw_modbus_client_request(lb.cli, req_slot, &frame_len);
        size_t written = 0;
        egw_err_t err = egw_transport_write(lb.cli_h, frame,
                                              &written, frame_len);
        if (err != EGW_OK || written != frame_len) {
            EGW_LOGE("  client write failed: err=%d", (int)err);
            goto cleanup;
        }
        EGW_LOGI("  client: → sent %zu bytes", written);
    }

    /* 5. uv_poll 监听 server fd（等从站收请求） */
    int srv_fd = egw_transport_get_fd(lb.srv_h);
    int cli_fd = egw_transport_get_fd(lb.cli_h);

    uv_poll_init(&loop, &lb.srv_poll, srv_fd);
    uv_poll_init(&loop, &lb.cli_poll, cli_fd);
    lb.srv_poll.data = &lb;
    lb.cli_poll.data = &lb;
    lb.phase = PHASE_SERVER_RECV;
    lb.seg_pending = false;

    uv_poll_start(&lb.srv_poll, UV_READABLE, on_srv_poll);

    /* 超时定时器 (2s) */
    uv_timer_init(&loop, &timer);
    timer.data = &lb;
    uv_timer_start(&timer, on_timeout, 2000, 0);

    /* 7. 运行事件循环 */
    EGW_LOGI("  uv_run start...");
    uv_run(&loop, UV_RUN_DEFAULT);
    EGW_LOGI("  uv_run done (phase=%d)", lb.phase);

cleanup:
    if (lb.phase != PHASE_DONE) {
        EGW_LOGE("  loopback did not complete cleanly");
    }

    /* 关闭 uv handles */
    uv_close((uv_handle_t *)&lb.srv_poll, NULL);
    uv_close((uv_handle_t *)&lb.cli_poll, NULL);
    uv_close((uv_handle_t *)&timer, NULL);
    uv_run(&loop, UV_RUN_DEFAULT);
    uv_loop_close(&loop);

    egw_modbus_client_destroy(lb.cli);
    egw_modbus_server_destroy(lb.server);
    egw_transport_close(lb.cli_h);
    egw_transport_close(lb.srv_h);
    EGW_LOGI("=== loopback done ===");
}

int egw_app_run(int argc, char *argv[])
{
    const char *db_path = "config.db";
    if (argc > 2) {
        db_path = argv[1];
    }

    egw_head_t *head = egw_ptable_head_load(db_path);
    if (!head) {
        EGW_LOGE("head_load failed (run: python tools/init_db.py %s)", db_path);
        return 1;
    }

    egw_ptable_t *pt = egw_ptable_open(db_path, head->version);
    if (!pt) {
        EGW_LOGE("ptable_open failed");
        egw_ptable_head_free(head);
        return 1;
    }

    EGW_LOGI("head.desc = %s", head->desc);
    for (egw_thread_t *th = head->threads; th; th = th->next) {
        EGW_LOGI("thread[%d] desc=%s", th->thread_id, th->desc);
        for (egw_node_t *n = th->nodes; n; n = n->next) {
            switch (n->type) {
            case EGW_THREAD_NODE_PROTOCOL:
                on_protocol_node(pt, n);
                break;
            case EGW_THREAD_NODE_PORT:
                on_port_node(n);
                break;
            case EGW_THREAD_NODE_SQLITE:
                EGW_LOGI("  sqlite = %s", n->desc[0] ? n->desc : "(empty)");
                break;
            }
        }
    }

    egw_ptable_close(pt);
    egw_ptable_head_free(head);

    /* 本地回环 Modbus 收发演示（uv_poll 驱动） */
    run_modbus_loopback();

    EGW_LOGI("done");
    return 0;
}
