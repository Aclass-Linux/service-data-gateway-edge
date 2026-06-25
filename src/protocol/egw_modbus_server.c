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
    egw_modbus_ser_wrap_fn    wrap;         /* RTU/TCP 帧打包 */
    egw_modbus_ser_unwrap_fn  unwrap;       /* RTU/TCP 帧解包 */

    uint8_t                 *buf;           /* 环形缓冲区（堆分配） */
    size_t                  cap;            /* 容量 */
    size_t                  rd;             /* 已消费字节数（帧定界后推进） */
    size_t                  wr;             /* 已累积字节数 */

    egw_modbus_srv_read_cb  read_cb;        /* 全部 unit 共用同一套回调 */
    egw_modbus_srv_write_cb write_cb;
    void                   *cb_arg;

    uint8_t                 unit_mask[32];  /* bit[i]=1 → unit_id=i 已注册 */

    bool                    sending;        /* true=响应待发送，禁止处理新帧 */
    uint8_t                 resp_buf[EGW_MODBUS_MAX_FRAME]; /* 已组装好的响应帧 */
    size_t                  resp_len;       /* 响应帧实际字节数 */
};

/* ── 位图辅助 ────────────────────────────────────────── */

static inline bool unit_is_active(const egw_modbus_server_t *s, uint8_t id)
{
    return (s->unit_mask[id / 8] >> (id % 8)) & 1u;
}

static inline void unit_set_active(egw_modbus_server_t *s, uint8_t id)
{
    s->unit_mask[id / 8] |= (uint8_t)(1u << (id % 8));
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

static void send_exception(egw_modbus_server_t *s, uint8_t unit_id,
                            uint8_t fc, uint8_t exc)
{
    uint8_t pdu[8];
    size_t plen = egw_modbus_build_exception_pdu(pdu, fc, exc);
    s->resp_len = s->wrap(s->resp_buf, unit_id, pdu, plen);
    s->sending = true;
    log_hex("send", s->resp_buf, s->resp_len);
}

/* ── 读请求处理 ───────────────────────────────────────── */

static egw_err_t handle_read_request(egw_modbus_server_t *s,
                                      uint8_t fc,
                                      uint16_t addr, uint16_t count,
                                      uint8_t unit_id,
                                      uint8_t *resp_pdu,
                                      size_t *resp_pdu_len)
{
    uint16_t regs[256];
    egw_err_t err = s->read_cb(addr, count, regs, unit_id, s->cb_arg);
    if (err != EGW_OK) { return err; }

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

    *resp_pdu_len = egw_modbus_build_read_resp_pdu(resp_pdu, fc, raw, raw_len);
    return (*resp_pdu_len > 0) ? EGW_OK : EGW_RET_CODE(ERR_INVALID_ARG);
}

/* ── 写请求处理 ───────────────────────────────────────── */

static egw_err_t handle_write_request(egw_modbus_server_t *s,
                                       uint8_t fc,
                                       uint16_t addr, uint16_t count,
                                       const uint8_t *req_data,
                                       const uint8_t *orig_pdu,
                                       size_t orig_pdu_len,
                                       uint8_t unit_id,
                                       uint8_t *resp_pdu,
                                       size_t *resp_pdu_len)
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

    if (s->write_cb && nregs > 0) {
        egw_err_t err = s->write_cb(addr, nregs, wregs, unit_id, s->cb_arg);
        if (err != EGW_OK) { return err; }
    }

    memcpy(resp_pdu, orig_pdu, orig_pdu_len);
    *resp_pdu_len = orig_pdu_len;
    return EGW_OK;
}

/* ── 广播写 ──────────────────────────────────────────── */

static void handle_broadcast_write(egw_modbus_server_t *s,
                                    uint8_t fc,
                                    uint16_t addr, uint16_t count,
                                    const uint8_t *req_data,
                                    const uint8_t *req_pdu,
                                    size_t req_pdu_len)
{
    uint8_t dummy_pdu[EGW_MODBUS_MAX_PDU];
    size_t  dummy_len = 0;
    for (uint16_t id = 1; id <= 247; id++) {
        if (!unit_is_active(s, (uint8_t)id)) { continue; }
        if (!s->write_cb) { continue; }
        handle_write_request(s, fc, addr, count, req_data,
                              req_pdu, req_pdu_len,
                              (uint8_t)id, dummy_pdu, &dummy_len);
    }
}

/* ── 前置声明 ───────────────────────────────────────── */

static void server_handle_frame(egw_modbus_server_t *s,
                                 const uint8_t *frame, size_t frame_len);

/* ── Server（从站）生命周期 ─────────────────────────── */

egw_modbus_server_t *egw_modbus_server_create(
    const egw_modbus_server_params_t *params)
{
    if (!params) { return NULL; }
    egw_modbus_server_t *s = calloc(1, sizeof(*s));
    if (!s) { return NULL; }
    s->buf = malloc(EGW_MODBUS_MAX_FRAME);
    if (!s->buf) { free(s); return NULL; }
    s->cap = EGW_MODBUS_MAX_FRAME;

    switch (params->transport) {
    case EGW_MODBUS_RTU:
        s->wrap   = egw_modbus_ser_wrap_rtu;
        s->unwrap = egw_modbus_ser_unwrap_rtu;
        break;
    case EGW_MODBUS_TCP:
        s->wrap   = egw_modbus_ser_wrap_tcp;
        s->unwrap = egw_modbus_ser_unwrap_tcp;
        break;
    }

    s->read_cb  = params->read_cb;
    s->write_cb = params->write_cb;
    s->cb_arg   = params->cb_arg;

    if (params->unit_id >= 1 && params->unit_id <= 247) {
        unit_set_active(s, params->unit_id);
    }
    return s;
}

egw_err_t egw_modbus_server_add_unit(egw_modbus_server_t *s,
                                      uint8_t unit_id)
{
    if (!s) { return EGW_RET_CODE(ERR_INVALID_ARG); }
    if (unit_id < 1 || unit_id > 247) { return EGW_RET_CODE(ERR_INVALID_ARG); }
    if (unit_is_active(s, unit_id)) { return EGW_RET_CODE(ERR_INVALID_ARG); }
    unit_set_active(s, unit_id);
    return EGW_OK;
}

void egw_modbus_server_destroy(egw_modbus_server_t *s)
{
    if (!s) { return; }
    free(s->buf);
    free(s);
}

/* ── 尝试处理缓冲区中的帧 ────────────────────────────── */

static void server_handle_frame(egw_modbus_server_t *s,
                                 const uint8_t *frame, size_t frame_len);

static void try_process(egw_modbus_server_t *s)
{
    if (s->sending) { return; }
    size_t remaining = s->wr - s->rd;
    if (remaining == 0) { return; }

    size_t frame_len = 0;
    egw_proto_result_t r = parse_and_check(s->buf + s->rd, remaining,
                                            &frame_len);
    if (r == EGW_PROTO_NEED_MORE) { return; }
    if (r == EGW_PROTO_FRAME_ERROR) {
        s->rd = s->wr = 0;
        return;
    }

    /* 取出完整帧，推进 rd，再处理（允许后续喂入不阻塞） */
    uint8_t frame[EGW_MODBUS_MAX_FRAME];
    memcpy(frame, s->buf + s->rd, frame_len);
    s->rd += frame_len;
    server_handle_frame(s, frame, frame_len);
}

/* ── 数据摄入 ────────────────────────────────────────── */

void egw_modbus_server_feed(egw_modbus_server_t *s,
                              const uint8_t *data, size_t len)
{
    if (!s || !data || len == 0) { return; }
    size_t avail = s->cap - s->wr;
    if (len > avail) { /* 环形溢出，丢弃所有未处理字节 */
        s->rd = s->wr = 0;
        return;
    }
    memcpy(s->buf + s->wr, data, len);
    s->wr += len;
    try_process(s);
}

uint8_t *egw_modbus_server_reserve(egw_modbus_server_t *s, size_t *avail)
{
    if (!s || s->sending || !avail) {
        if (avail) { *avail = 0; }
        return NULL;
    }
    *avail = s->cap - s->wr;
    return s->buf + s->wr;
}

void egw_modbus_server_commit(egw_modbus_server_t *s, size_t n)
{
    if (!s || n == 0 || s->sending) { return; }
    if (s->wr + n > s->cap) { s->rd = s->wr = 0; return; }
    s->wr += n;
    try_process(s);
}

bool egw_modbus_server_response_ready(const egw_modbus_server_t *s)
{
    return s ? s->resp_len > 0 : false;
}

const uint8_t *egw_modbus_server_get_response(egw_modbus_server_t *s,
                                               size_t *len)
{
    if (!s || !len) { return NULL; }
    *len = s->resp_len;
    return s->resp_buf;
}

void egw_modbus_server_response_sent(egw_modbus_server_t *s)
{
    if (!s) { return; }
    s->resp_len = 0;
    s->sending = false;
    /* 保留未处理字节，紧致到头部 */
    size_t remaining = s->wr - s->rd;
    if (remaining > 0) {
        memmove(s->buf, s->buf + s->rd, remaining);
    }
    s->wr = remaining;
    s->rd = 0;
    try_process(s);
}

/* ── Server（从站）：帧已就绪后的处理 + 响应生成 ─────── */

static void server_handle_frame(egw_modbus_server_t *s,
                                 const uint8_t *frame, size_t frame_len)
{
    uint8_t req_pdu[EGW_MODBUS_MAX_PDU];
    size_t  req_pdu_len = 0;
    uint8_t req_unit_id = 0;

    if (s->unwrap(frame, frame_len, &req_unit_id, req_pdu,
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
        send_exception(s, req_unit_id, fc, EGW_MODBUS_EXC_ILLEGAL_DATA_ADDR);
        return;
    }

    if (req_unit_id == 0) {
        if (fc == 0x05 || fc == 0x06 || fc == 0x0F || fc == 0x10) {
            handle_broadcast_write(s, fc, addr, count,
                                    req_data, req_pdu, req_pdu_len);
        }
        return;
    }

    if (!unit_is_active(s, req_unit_id)) { return; }

    uint8_t resp_pdu[EGW_MODBUS_MAX_PDU];
    size_t  resp_pdu_len = 0;

    switch (fc) {
    case 0x01: case 0x02: case 0x03: case 0x04:
        if (!s->read_cb) {
            send_exception(s, req_unit_id, fc,
                            EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE);
            return;
        }
        if (handle_read_request(s, fc, addr, count, req_unit_id,
                                 resp_pdu, &resp_pdu_len) != EGW_OK) {
            resp_pdu_len = egw_modbus_build_exception_pdu(
                resp_pdu, fc, EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE);
        }
        break;
    case 0x05: case 0x06: case 0x0F: case 0x10:
        if (handle_write_request(s, fc, addr, count,
                                  req_data, req_pdu, req_pdu_len,
                                  req_unit_id,
                                  resp_pdu, &resp_pdu_len) != EGW_OK) {
            resp_pdu_len = egw_modbus_build_exception_pdu(
                resp_pdu, fc, EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE);
        }
        break;
    default:
        resp_pdu_len = egw_modbus_build_exception_pdu(
            resp_pdu, fc, EGW_MODBUS_EXC_ILLEGAL_FUNCTION);
        break;
    }

    if (resp_pdu_len == 0) { return; }

    s->resp_len = s->wrap(s->resp_buf, req_unit_id,
                                         resp_pdu, resp_pdu_len);
    s->sending = true;
    log_hex("send", s->resp_buf, s->resp_len);
}
