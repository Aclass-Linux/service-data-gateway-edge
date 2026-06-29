#include "egw_modbus_master.h"
#include <time.h>

/* ── 南向采集字段映射 ────────────────────────────────── */

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
    EGW_FIELD(egw_modbus_master_t, NULL,         value,         EGW_CTYPE_U16),
};

/* ── 南向采集配置加载 ────────────────────────────────── */

static int master_lkp_cmp(const void *key, const void *row)
{
    const egw_modbus_master_key_t *k = key;
    const egw_modbus_master_t *r = row;
    uint32_t va = ((uint32_t)k->unit_id << 16) | k->reg_addr;
    uint32_t vb = ((uint32_t)r->device_id << 16) | r->reg_addr;
    return (va > vb) - (va < vb);
}

egw_ptable_rs_t *egw_modbus_master_load(egw_ptable_t *pt)
{
    size_t nf = sizeof(s_master_fields) / sizeof(s_master_fields[0]);

    egw_ptable_rs_t *rs = egw_ptable_register(pt, "southbound",
        &(egw_schema_t){
            .fields   = s_master_fields,
            .nfields  = nf,
            .row_size = sizeof(egw_modbus_master_t),
            .order_by = "device_id, reg_addr",
            .lkp      = master_lkp_cmp,
        });
    if (!rs) { return NULL; }

    EGW_LOGI("  register southbound: loaded %zu row(s)",
             egw_ptable_rs_count(rs));
    return rs;
}

/* ── 主站完成回调 ──────────────────────────────────── */

static void master_done_cb(const egw_modbus_result_t *result, void *arg)
{
    egw_lb_ctx_t *ctx = arg;

    if (result->reg_count < 0) {
        EGW_LOGE("    [client] done_cb: error unit=%u addr=%u",
                 result->unit_id, result->addr);
        uv_stop(ctx->loop);
        return;
    }
    EGW_LOGI("    [client] done_cb: unit=%u addr=%u regs[0]=0x%04X regs[1]=0x%04X",
             result->unit_id, result->addr,
             result->regs[0], result->reg_count > 1 ? result->regs[1] : 0);
    ctx->phase = EGW_LB_PHASE_DONE;
    uv_stop(ctx->loop);
}

/* ── 主站初始化：创建 client + 注册请求 ─────────────── */

egw_err_t egw_lb_master_init(egw_lb_ctx_t *ctx, egw_ptable_rs_t *rs)
{
    ctx->cli = egw_modbus_client_create(&(egw_modbus_client_params_t){
        .transport = EGW_MODBUS_RTU,
        .done_cb   = master_done_cb,
        .cb_arg    = ctx,
    });
    if (!ctx->cli) {
        return EGW_RET_CODE(ERR_NOMEM);
    }

    const egw_modbus_master_t *m0 = egw_ptable_rs_row(rs, 0);
    ctx->req_slot = egw_modbus_client_register(ctx->cli,
        &(egw_modbus_encode_params_t){
            .unit_id  = m0->device_id,
            .funccode = m0->func_code,
            .addr     = m0->reg_addr,
            .count    = m0->reg_count,
        });
    if (!ctx->req_slot) {
        return EGW_RET_CODE(ERR_NOMEM);
    }
    EGW_LOGI("  client: req built (unit=%u fc=%u addr=%u qty=%u)",
             m0->device_id, m0->func_code, m0->reg_addr, m0->reg_count);
    return EGW_OK;
}

/* ── 主站发送请求 ───────────────────────────────────── */

egw_err_t egw_lb_master_send(egw_lb_ctx_t *ctx)
{
    size_t frame_len = 0;
    const uint8_t *frame = egw_modbus_client_request(ctx->cli,
        ctx->req_slot, &frame_len);
    size_t written = 0;
    egw_err_t err = egw_transport_write(ctx->cli_h, frame,
                                        &written, frame_len);
    if (err != EGW_OK || written != frame_len) {
        return EGW_RET_CODE(ERR_WRITE);
    }
    EGW_LOGI("  client: → sent %zu bytes", written);
    return EGW_OK;
}

/* ── client fd 可读回调 ─────────────────────────────── */

/*
 * 零拷贝：transport 直接读入 client 的接收缓冲区。
 * 如果还有后半段未发（seg_pending），先发后半段再接收。
 */
void egw_lb_master_on_poll(uv_poll_t *p, int status, int events)
{
    egw_lb_ctx_t *ctx = p->data;

    if (status < 0 || !(events & UV_READABLE)) {
        return;
    }

    uint8_t *wp = NULL;
    size_t avail = egw_modbus_client_reserve(ctx->cli, &wp);
    if (!wp || avail == 0) {
        return;
    }

    size_t rlen = 0;
    if (egw_transport_read(ctx->cli_h, wp, &rlen, avail) != EGW_OK
        || rlen == 0) {
        return;
    }

    egw_modbus_client_commit(ctx->cli, rlen);

    if (ctx->seg_pending) {
        ctx->seg_pending = false;
        size_t resp_len = 0;
        const uint8_t *resp = egw_modbus_server_get_response(ctx->server,
                                                              &resp_len);
        if (resp) {
            size_t first_half = resp_len / 2;
            size_t written = 0;
            egw_transport_write(ctx->srv_h, resp + first_half,
                                &written, resp_len - first_half);
            EGW_LOGI("    [server] → sent %zu/%zu bytes (second half)",
                     written, resp_len - first_half);
            egw_modbus_server_response_sent(ctx->server);
        }
    }
}

/* ── 超时定时器 ─────────────────────────────────────── */

void egw_lb_on_timeout(uv_timer_t *t)
{
    egw_lb_ctx_t *ctx = t->data;
    EGW_LOGE("  loopback timeout (phase=%d)", ctx->phase);
    uv_stop(ctx->loop);
}

/* ── 主站传输打开 ───────────────────────────────────── */

egw_err_t egw_lb_master_transport_open(egw_lb_ctx_t *ctx, const char *path)
{
    egw_transport_serial_params_t sp = {
        .baud = 9600, .parity = 'N', .data_bits = 8, .stop_bits = 1,
    };
    sp.path = path;
    ctx->cli_h = egw_transport_serial_open(&sp);
    if (!ctx->cli_h) {
        return EGW_RET_CODE(ERR_OPEN);
    }
    return EGW_OK;
}

/* ── 主站 uv_poll 初始化 ────────────────────────────── */

void egw_lb_master_poll_init(egw_lb_ctx_t *ctx)
{
    int fd = egw_transport_get_fd(ctx->cli_h);
    uv_poll_init(ctx->loop, &ctx->cli_poll, fd);
    ctx->cli_poll.data = ctx;
}

/* ── 主站资源清理 ───────────────────────────────────── */

void egw_lb_master_cleanup(egw_lb_ctx_t *ctx)
{
    if (ctx->cli) {
        egw_modbus_client_destroy(ctx->cli);
        ctx->cli = NULL;
    }
    if (ctx->cli_h) {
        egw_transport_close(ctx->cli_h);
        ctx->cli_h = NULL;
    }
}
