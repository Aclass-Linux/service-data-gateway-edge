#include "egw_modbus_server.h"
#include "egw_crc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MIN_FRAME 4

/* ── Modbus 帧长度推定（Server 侧固定为 REQUEST） ─── */

static int modbus_frame_len(const uint8_t *buf, size_t len)
{
    if (len < 2) { return -1; }
    uint8_t func = buf[1];
    if (func & 0x80u) { return 5; }

    switch (func) {
    case 0x01: case 0x02: case 0x03: case 0x04:
    case 0x05: case 0x06:
        return 8;
    case 0x0F: case 0x10:
        if (len < 7) { return -1; }
        return 9 + (int)buf[6];
    default:
        return -2;
    }
}

static bool modbus_validate(const uint8_t *buf, size_t frame_len)
{
    uint16_t calc = egw_crc_modbus_table(buf, frame_len - 2u);
    uint16_t recv = (uint16_t)buf[frame_len - 2]
                  | ((uint16_t)buf[frame_len - 1] << 8);
    return calc == recv;
}

/* ── 内联帧定界（替代 egw_proto_handle） ──────────────── */

static egw_proto_result_t parse_and_check(const uint8_t *buf, size_t len,
                                           size_t *frame_len)
{
    if (len < MIN_FRAME) { return EGW_PROTO_NEED_MORE; }
    int exp = modbus_frame_len(buf, len);
    if (exp == -1) { return EGW_PROTO_NEED_MORE; }
    if (exp < 0) { return EGW_PROTO_FRAME_ERROR; }
    if (len < (size_t)exp) { return EGW_PROTO_NEED_MORE; }
    if (!modbus_validate(buf, (size_t)exp)) { return EGW_PROTO_FRAME_ERROR; }
    *frame_len = (size_t)exp;
    return EGW_PROTO_FRAME_READY;
}

/* ── Server（从站）内部结构 ──────────────────────────── */

struct egw_modbus_server {
    egw_modbus_transport_t    transport;

    uint8_t                 *buf;       /* 接收环形缓冲区 */
    size_t                  buf_cap;
    size_t                  rd;
    size_t                  wr;

    uint8_t                 *resp_buf;  /* 响应缓冲区 */
    size_t                  resp_cap;
    size_t                  resp_len;   /* 响应总长（>0 即有待发送） */

    egw_modbus_srv_read_cb  read_cb;
    egw_modbus_srv_write_cb write_cb;
    void                   *cb_arg;

    uint8_t                 unit_mask[32];

    bool                    sending;
};

/* ── 位图辅助 ────────────────────────────────────────── */

static inline bool unit_is_active(const egw_modbus_server_t *server, uint8_t id)
{
    return (server->unit_mask[id / 8] >> (id % 8)) & 1u;
}

static inline void unit_set_active(egw_modbus_server_t *server, uint8_t id)
{
    server->unit_mask[id / 8] |= (uint8_t)(1u << (id % 8));
}

/* ── Hex 日志 ────────────────────────────────────────── */

static void log_hex(const char *tag, const uint8_t *buf, size_t len)
{
    if (len == 0) { return; }
    size_t show = len > 64 ? 64 : len;
    char hex[64 * 3 + 8];
    size_t pos = 0;
    for (size_t i = 0; i < show; i++) {
        pos += snprintf(hex + pos, sizeof(hex) - pos,
                         "%s%02x", i == 0 ? "" : " ", buf[i]);
        if (pos >= sizeof(hex) - 4) { break; }
    }
    if (len > 64) {
        snprintf(hex + pos, sizeof(hex) - pos, " ...");
    }
    EGW_LOGI("  [server] %s (%zu): %s", tag, len, hex);
}

/* ── 发异常响应（用帧里的原始 unit_id） ──────────────── */

static void send_exception(egw_modbus_server_t *server, uint8_t unit_id,
                            uint8_t fc, uint8_t exc)
{
    egw_modbus_encode_params_t enc;
    memset(&enc, 0, sizeof(enc));
    enc.type     = EGW_ENCODE_EXCEPTION;
    enc.unit_id  = unit_id;
    enc.funccode = fc;
    enc.exc_code = exc;
    enc.buf      = server->resp_buf;
    enc.cap      = server->resp_cap;

    uint8_t *out = egw_modbus_encode(server->transport, &enc, &server->resp_len);
    if (!out || server->resp_len == 0) { return; }

    server->sending = true;
    log_hex("send", server->resp_buf, server->resp_len);
}

/* ── 读请求处理（构建读响应 PDU，返回 PDU 长度） ──────── */

static size_t handle_read_request(egw_modbus_server_t *server,
                                   uint8_t fc,
                                   uint16_t addr, uint16_t count,
                                   uint8_t unit_id,
                                   uint8_t *resp_pdu)
{
    uint16_t regs[256];
    if (server->read_cb(addr, count, regs, unit_id, server->cb_arg) != EGW_OK) {
        return 0;
    }

    uint8_t raw[512];
    size_t raw_len = 0;

    if (fc == 0x01 || fc == 0x02) {
        raw_len = (size_t)((count + 7) / 8);
        for (uint16_t i = 0; i < raw_len; i++) { raw[i] = (uint8_t)regs[i]; }
    } else {
        raw_len = (size_t)count * 2;
        for (uint16_t i = 0; i < count; i++) {
            raw[i * 2]     = (uint8_t)(regs[i] >> 8);
            raw[i * 2 + 1] = (uint8_t)(regs[i] & 0xFF);
        }
    }

    return egw_modbus_build_read_resp_pdu(resp_pdu, fc, raw, raw_len);
}

/* ── 写请求处理（返回 true=成功） ─────────────────────── */

static bool handle_write_request(egw_modbus_server_t *server,
                                  uint8_t fc,
                                  uint16_t addr, uint16_t count,
                                  const uint8_t *req_data,
                                  uint8_t unit_id)
{
    uint16_t wregs[256];
    uint16_t nregs = 0;

    if (req_data) {
        if (fc == 0x05 || fc == 0x06) {
            wregs[0] = (uint16_t)req_data[0] << 8 | req_data[1];
            nregs = 1;
        } else if (fc == 0x0F) {
            nregs = (count + 15) / 16;
            for (uint16_t i = 0; i < nregs && i < 256; i++) { wregs[i] = 0; }
            for (uint16_t b = 0; b < count && b < 2000; b++) {
                uint16_t word = b / 16;
                uint8_t  bit  = (uint8_t)(b % 16);
                if (word < 256 && (req_data[b / 8] & (1u << (b % 8)))) {
                    wregs[word] |= (uint16_t)(1u << bit);
                }
            }
        } else {
            nregs = count;
            for (uint16_t i = 0; i < count && i < 256; i++) {
                wregs[i] = (uint16_t)req_data[i * 2] << 8
                         | req_data[i * 2 + 1];
            }
        }
    }

    return !server->write_cb || server->write_cb(addr, nregs, wregs, unit_id, server->cb_arg) == EGW_OK;
}

/* ── 广播写 ──────────────────────────────────────────── */

static void handle_broadcast_write(egw_modbus_server_t *server,
                                    uint8_t fc,
                                    uint16_t addr, uint16_t count,
                                    const uint8_t *req_data)
{
    for (uint16_t id = 1; id <= 247; id++) {
        if (!unit_is_active(server, (uint8_t)id)) { continue; }
        if (!server->write_cb) { continue; }
        handle_write_request(server, fc, addr, count, req_data, (uint8_t)id);
    }
}

/* ── 前置声明 ───────────────────────────────────────── */

static void server_handle_frame(egw_modbus_server_t *server,
                                 const uint8_t *frame, size_t frame_len);

/* ── Server（从站）生命周期 ─────────────────────────── */

egw_modbus_server_t *egw_modbus_server_create(
    const egw_modbus_server_params_t *params)
{
    if (!params) { return NULL; }
    egw_modbus_server_t *server = calloc(1, sizeof(*server));
    if (!server) { return NULL; }

    size_t buf_cap = params->buf_cap ? params->buf_cap : EGW_MODBUS_MAX_FRAME;
    size_t resp_cap = params->resp_cap ? params->resp_cap : EGW_MODBUS_MAX_FRAME;

    server->buf = malloc(buf_cap);
    if (!server->buf) { free(server); return NULL; }
    server->resp_buf = malloc(resp_cap);
    if (!server->resp_buf) { free(server->buf); free(server); return NULL; }

    server->buf_cap = buf_cap;
    server->resp_cap = resp_cap;

    server->transport = params->transport;

    server->read_cb  = params->read_cb;
    server->write_cb = params->write_cb;
    server->cb_arg   = params->cb_arg;

    for (int i = 0; i < 4 && params->unit_ids[i] != 0; i++) {
        unit_set_active(server, params->unit_ids[i]);
    }
    return server;
}

egw_err_t egw_modbus_server_add_unit(egw_modbus_server_t *server,
                                      uint8_t unit_id)
{
    if (!server) { return EGW_RET_CODE(ERR_INVALID_ARG); }
    if (unit_id < 1 || unit_id > 247) { return EGW_RET_CODE(ERR_INVALID_ARG); }
    if (unit_is_active(server, unit_id)) { return EGW_RET_CODE(ERR_INVALID_ARG); }
    unit_set_active(server, unit_id);
    return EGW_OK;
}

void egw_modbus_server_destroy(egw_modbus_server_t *server)
{
    if (!server) { return; }
    free(server->buf);
    free(server->resp_buf);
    free(server);
}

/* ── 环形缓冲区辅助 ────────────────────────────────── */

/** @brief 已写入但未处理字节数 */
static size_t buf_used(const egw_modbus_server_t *s)
{
    if (s->wr >= s->rd) { return s->wr - s->rd; }
    return s->buf_cap - s->rd + s->wr;
}

/** @brief 可写入字节数（留一个空位区分满/空） */
static size_t buf_free(const egw_modbus_server_t *s)
{
    return s->buf_cap - buf_used(s) - 1;
}

/** @brief 将环形缓冲区 [rd, rd+len) 拷贝到连续 dst，处理回绕 */
static void ring_copy(uint8_t *dst, const uint8_t *buf, size_t cap,
                      size_t rd, size_t len)
{
    size_t to_end = cap - rd;
    if (len <= to_end) {
        memcpy(dst, buf + rd, len);
    } else {
        memcpy(dst, buf + rd, to_end);
        memcpy(dst + to_end, buf, len - to_end);
    }
}

/* ── 尝试处理缓冲区中的帧 ────────────────────────────── */

static void server_handle_frame(egw_modbus_server_t *server,
                                 const uint8_t *frame, size_t frame_len);

static void try_process(egw_modbus_server_t *server)
{
    /* 有响应待发送中，先不发新帧 */
    if (server->sending) { return; }

    size_t used = buf_used(server);
    if (used == 0) { return; }

    /* 把可用数据拷贝到连续 tmp 中（处理回绕），再定界 */
    size_t copy_sz = (used > EGW_MODBUS_MAX_FRAME) ? EGW_MODBUS_MAX_FRAME : used;
    uint8_t tmp[EGW_MODBUS_MAX_FRAME];
    ring_copy(tmp, server->buf, server->buf_cap, server->rd, copy_sz);

    size_t frame_len = 0;
    egw_proto_result_t r = parse_and_check(tmp, copy_sz, &frame_len);

    /* 还没收齐，等更多字节 */
    if (r == EGW_PROTO_NEED_MORE) { return; }

    /* CRC 失败或格式错误，丢弃全部未处理数据 */
    if (r == EGW_PROTO_FRAME_ERROR) {
        server->rd = server->wr = 0;
        return;
    }

    /* EGW_PROTO_FRAME_READY:
     * tmp 中已有完整帧，推进 rd（释放环形空间），
     * 再调 server_handle_frame 处理。
     * 先拷贝再推进 rd 的原因是：回调可能触发 feed 再入，
     * 提前推进 rd 保证后续字节不会覆盖当前帧 */
    server->rd = (server->rd + frame_len) % server->buf_cap;
    server_handle_frame(server, tmp, frame_len);
}

/* ── 数据摄入 ────────────────────────────────────────── */

void egw_modbus_server_feed(egw_modbus_server_t *server,
                              const uint8_t *data, size_t len)
{
    if (!server || !data || len == 0) { return; }

    size_t free_sz = buf_free(server);
    if (len > free_sz) {
        server->rd = server->wr = 0;
        return;
    }

    size_t to_end = server->buf_cap - server->wr;
    if (len <= to_end) {
        memcpy(server->buf + server->wr, data, len);
    } else {
        memcpy(server->buf + server->wr, data, to_end);
        memcpy(server->buf, data + to_end, len - to_end);
    }
    server->wr = (server->wr + len) % server->buf_cap;
    try_process(server);
}

uint8_t *egw_modbus_server_reserve(egw_modbus_server_t *server, size_t *avail)
{
    if (!server || server->sending || !avail) {
        if (avail) { *avail = 0; }
        return NULL;
    }

    size_t free_sz = buf_free(server);
    if (free_sz == 0) { *avail = 0; return NULL; }

    size_t contig;
    if (server->wr < server->rd) {
        contig = server->rd - server->wr - 1;
    } else {
        contig = server->buf_cap - server->wr;
    }

    *avail = (contig < free_sz) ? contig : free_sz;
    return server->buf + server->wr;
}

void egw_modbus_server_commit(egw_modbus_server_t *server, size_t n)
{
    if (!server || n == 0 || server->sending) { return; }

    size_t contig;
    if (server->wr < server->rd) {
        contig = server->rd - server->wr - 1;
    } else {
        contig = server->buf_cap - server->wr;
    }
    if (n > contig) { return; }

    server->wr = (server->wr + n) % server->buf_cap;
    try_process(server);
}

const uint8_t *egw_modbus_server_get_response(egw_modbus_server_t *server,
                                               size_t *len)
{
    if (!server || !len) { return NULL; }
    if (server->resp_len == 0) { *len = 0; return NULL; }
    *len = server->resp_len;
    return server->resp_buf;
}

void egw_modbus_server_response_sent(egw_modbus_server_t *server)
{
    if (!server) { return; }
    server->resp_len = 0;
    server->sending = false;
    try_process(server);
}

/* ── Server（从站）：帧已就绪后的处理 + 响应生成 ─────── */

static void server_handle_frame(egw_modbus_server_t *server,
                                 const uint8_t *frame, size_t frame_len)
{
    uint8_t req_pdu[EGW_MODBUS_MAX_PDU];
    size_t  req_pdu_len = 0;
    uint8_t req_unit_id = 0;

    if (egw_modbus_decode(server->transport, frame, frame_len,
                            &req_unit_id, req_pdu,
                            &req_pdu_len) != EGW_OK) {
        return;
    }

    log_hex("recv", frame, frame_len);

    uint8_t      fc = 0;
    uint16_t     addr = 0, count = 0;
    const uint8_t *req_data = NULL;
    size_t        req_data_len = 0;

    if (egw_modbus_parse_request(req_pdu, req_pdu_len,
                                  &fc, &addr, &count,
                                  &req_data, &req_data_len) != EGW_OK) {
        send_exception(server, req_unit_id, fc, EGW_MODBUS_EXC_ILLEGAL_DATA_ADDR);
        return;
    }

    if (req_unit_id == 0) {
        if (fc == 0x05 || fc == 0x06 || fc == 0x0F || fc == 0x10) {
            handle_broadcast_write(server, fc, addr, count,
                                    req_data);
        }
        return;
    }

    if (!unit_is_active(server, req_unit_id)) { return; }

    uint8_t resp_pdu[EGW_MODBUS_MAX_PDU];
    size_t  resp_pdu_len = 0;

    switch (fc) {
    case 0x01: case 0x02: case 0x03: case 0x04:
        if (!server->read_cb) {
            send_exception(server, req_unit_id, fc,
                            EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE);
            return;
        }
        resp_pdu_len = handle_read_request(server, fc, addr, count,
                                            req_unit_id, resp_pdu);
        if (resp_pdu_len == 0) {
            send_exception(server, req_unit_id, fc,
                            EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE);
            return;
        }
        break;
    case 0x05: case 0x06: case 0x0F: case 0x10:
        if (!handle_write_request(server, fc, addr, count,
                                   req_data, req_unit_id)) {
            send_exception(server, req_unit_id, fc,
                            EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE);
            return;
        }
        /* 回显请求 PDU */
        memcpy(resp_pdu, req_pdu, req_pdu_len);
        resp_pdu_len = req_pdu_len;
        break;
    default:
        send_exception(server, req_unit_id, fc,
                        EGW_MODBUS_EXC_ILLEGAL_FUNCTION);
        return;
    }

    egw_modbus_encode_params_t enc;
    memset(&enc, 0, sizeof(enc));
    enc.type    = EGW_ENCODE_PDU;
    enc.unit_id = req_unit_id;
    enc.pdu     = resp_pdu;
    enc.pdu_len = resp_pdu_len;
    enc.buf     = server->resp_buf;
    enc.cap     = server->resp_cap;

    uint8_t *out = egw_modbus_encode(server->transport, &enc, &server->resp_len);
    if (!out || server->resp_len == 0) { return; }

    server->sending = true;
    log_hex("send", server->resp_buf, server->resp_len);
}