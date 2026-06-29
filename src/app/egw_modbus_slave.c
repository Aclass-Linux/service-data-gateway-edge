#include "egw_modbus_slave.h"
#include <time.h>

/* ── 北向服务字段映射 ────────────────────────────────── */

static const egw_field_t s_slave_fields[] = {
    EGW_FIELD(egw_modbus_slave_t, "device_id",  device_id, EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_slave_t, "sig_id",     sig_id,    EGW_CTYPE_U32),
    EGW_FIELD(egw_modbus_slave_t, "func_code",  func_code, EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "reg_addr",   reg_addr,  EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_slave_t, "ctype",      ctype,     EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "flags",      flags,     EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "scale",      scale,     EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_slave_t, "offset",     offset,    EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_slave_t, "deadband",   deadband,  EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_slave_t, NULL,         value,  EGW_CTYPE_U16),
};

/* ── 北向服务配置加载 ────────────────────────────────── */

static int slave_lkp_cmp(const void *key, const void *row)
{
    const egw_modbus_slave_key_t *k = key;
    const egw_modbus_slave_t *r = row;
    uint32_t va = ((uint32_t)k->unit_id << 16) | k->reg_addr;
    uint32_t vb = ((uint32_t)r->device_id << 16) | r->reg_addr;
    return (va > vb) - (va < vb);
}

egw_ptable_rs_t *egw_modbus_slave_load(egw_ptable_t *pt)
{
    size_t nf = sizeof(s_slave_fields) / sizeof(s_slave_fields[0]);

    egw_ptable_rs_t *rs = egw_ptable_register(pt, "northbound",
        &(egw_schema_t){
            .fields   = s_slave_fields,
            .nfields  = nf,
            .row_size = sizeof(egw_modbus_slave_t),
            .order_by = "device_id, reg_addr",
            .lkp      = slave_lkp_cmp,
        });
    if (!rs) { return NULL; }

    EGW_LOGI("  register northbound: loaded %zu row(s)",
             egw_ptable_rs_count(rs));
    return rs;
}

/* ── 从站读回调 ─────────────────────────────────────── */

static egw_err_t slave_read_cb(const egw_modbus_srv_read_t *p, void *arg)
{
    const egw_ptable_rs_t *rs = arg;
    EGW_LOGI("    [server] read_cb: unit=%u addr=%u qty=%u",
             p->unit_id, p->address, p->quantity);

    egw_modbus_slave_key_t key = { .unit_id = p->unit_id,
                                     .reg_addr = p->address };
    egw_modbus_slave_t *row = egw_ptable_rs_lookup(rs, &key);
    if (!row) {
        EGW_LOGE("    [server] lookup miss unit=%u addr=%u",
                 p->unit_id, p->address);
        for (uint16_t i = 0; i < p->quantity; i++) {
            p->regs_out[i] = 0;
        }
        return EGW_OK;
    }

    for (uint16_t i = 0; i < p->quantity; i++) {
        p->regs_out[i] = 0x000A + row->reg_addr + i;
    }
    row->value = p->regs_out[0];
    EGW_LOGI("    [server] read_cb: unit=%u addr=%u qty=%u → value=0x%04X",
             p->unit_id, p->address, p->quantity, row->value);
    return EGW_OK;
}

/* ── 从站初始化 ─────────────────────────────────────── */

egw_err_t egw_lb_slave_init(egw_lb_ctx_t *ctx, egw_ptable_rs_t *rs)
{
    uint8_t unit_ids[5] = {0};
    size_t nu = 0;
    size_t nrow = egw_ptable_rs_count(rs);
    for (size_t i = 0; i < nrow && nu < 4; i++) {
        const egw_modbus_slave_t *s = egw_ptable_rs_row(rs, i);
        uint8_t uid = (uint8_t)s->device_id;
        int dup = 0;
        for (size_t j = 0; j < nu; j++) {
            if (unit_ids[j] == uid) { dup = 1; break; }
        }
        if (!dup) { unit_ids[nu++] = uid; }
    }

    ctx->server = egw_modbus_server_create(&(egw_modbus_server_params_t){
        .transport = EGW_MODBUS_RTU,
        .unit_ids  = { unit_ids[0], unit_ids[1], unit_ids[2], unit_ids[3] },
        .read_cb   = slave_read_cb,
        .cb_arg    = (void *)rs,
    });
    if (!ctx->server) {
        return EGW_RET_CODE(ERR_NOMEM);
    }

    EGW_LOGI("  server: created with %zu unit(s)", nu);
    return EGW_OK;
}

/* ── server fd 可读回调 ─────────────────────────────── */

/*
 * 零拷贝：transport 直接读入 server 的 protocol 缓冲区。
 * 收到请求后构建响应，模拟分段发送：
 *   前半段立即发，后半段交由 client poll 再发。
 */
void egw_lb_slave_on_poll(uv_poll_t *p, int status, int events)
{
    egw_lb_ctx_t *ctx = p->data;

    if (status < 0 || !(events & UV_READABLE)) {
        return;
    }

    uint8_t *wp = NULL;
    size_t avail = egw_modbus_server_reserve(ctx->server, &wp);
    if (!wp || avail == 0) {
        return;
    }

    size_t rlen = 0;
    if (egw_transport_read(ctx->srv_h, wp, &rlen, avail) != EGW_OK
        || rlen == 0) {
        return;
    }

    egw_modbus_server_commit(ctx->server, rlen);

    size_t resp_len = 0;
    const uint8_t *resp = egw_modbus_server_get_response(ctx->server,
                                                          &resp_len);
    if (!resp) {
        return;
    }
    EGW_LOGI("    [server] recv request, → response ready");

    size_t first_half = resp_len / 2;
    size_t written = 0;
    egw_transport_write(ctx->srv_h, resp, &written, first_half);
    EGW_LOGI("    [server] → sent %zu/%zu bytes (first half)",
             written, resp_len);

    uv_poll_stop(&ctx->srv_poll);
    ctx->seg_pending = true;
    uv_poll_start(&ctx->cli_poll, UV_READABLE, egw_lb_master_on_poll);
}

/* ── 从站传输打开 ───────────────────────────────────── */

egw_err_t egw_lb_slave_transport_open(egw_lb_ctx_t *ctx, const char *path)
{
    egw_transport_serial_params_t sp = {
        .baud = 9600, .parity = 'N', .data_bits = 8, .stop_bits = 1,
    };
    sp.path = path;
    ctx->srv_h = egw_transport_serial_open(&sp);
    if (!ctx->srv_h) {
        return EGW_RET_CODE(ERR_OPEN);
    }
    return EGW_OK;
}

/* ── 从站 uv_poll 初始化并开始监听 ──────────────────── */

void egw_lb_slave_poll_start(egw_lb_ctx_t *ctx)
{
    int fd = egw_transport_get_fd(ctx->srv_h);
    uv_poll_init(ctx->loop, &ctx->srv_poll, fd);
    ctx->srv_poll.data = ctx;
    uv_poll_start(&ctx->srv_poll, UV_READABLE, egw_lb_slave_on_poll);
}

/* ── 从站资源清理 ───────────────────────────────────── */

void egw_lb_slave_cleanup(egw_lb_ctx_t *ctx)
{
    if (ctx->server) {
        egw_modbus_server_destroy(ctx->server);
        ctx->server = NULL;
    }
    if (ctx->srv_h) {
        egw_transport_close(ctx->srv_h);
        ctx->srv_h = NULL;
    }
}
