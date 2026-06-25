#include "egw_modbus.h"
#include <string.h>
#include <stdlib.h>

/* ── 帧封装／解封装 ──────────────────────────────────── */

size_t egw_modbus_wrap_pdu(uint8_t *buf, egw_modbus_transport_t transport,
                            uint8_t unit_id,
                            const uint8_t *pdu, size_t pdu_len,
                            uint16_t tid)
{
    if (!buf || !pdu || pdu_len == 0) {
        return 0;
    }

    switch (transport) {
    case EGW_MODBUS_RTU: {
        if (pdu_len + 3 > EGW_MODBUS_MAX_FRAME) {
            return 0;
        }
        buf[0] = unit_id;
        memcpy(buf + 1, pdu, pdu_len);
        uint16_t crc = egw_crc_modbus_table(buf, pdu_len + 1);
        buf[pdu_len + 1] = (uint8_t)(crc & 0xFF);
        buf[pdu_len + 2] = (uint8_t)(crc >> 8);
        return pdu_len + 3;
    }
    case EGW_MODBUS_TCP: {
        if (pdu_len + 7 > EGW_MODBUS_MAX_FRAME) {
            return 0;
        }
        uint16_t mbap_len = (uint16_t)(pdu_len + 1);
        buf[0] = (uint8_t)(tid >> 8);
        buf[1] = (uint8_t)(tid & 0xFF);
        buf[2] = 0;
        buf[3] = 0;
        buf[4] = (uint8_t)(mbap_len >> 8);
        buf[5] = (uint8_t)(mbap_len & 0xFF);
        buf[6] = unit_id;
        memcpy(buf + 7, pdu, pdu_len);
        return pdu_len + 7;
    }
    }
    return 0;
}

egw_err_t egw_modbus_unwrap_frame(const uint8_t *frame, size_t len,
                                   egw_modbus_transport_t transport,
                                   uint8_t *unit_id_out,
                                   uint8_t *pdu_out, size_t *pdu_len_out)
{
    if (!frame || !unit_id_out || !pdu_out || !pdu_len_out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    switch (transport) {
    case EGW_MODBUS_RTU: {
        if (len < 4) {
            return EGW_RET_CODE(ERR_INVALID_ARG);
        }
        uint16_t crc_calc = egw_crc_modbus_table(frame, len - 2);
        uint16_t crc_recv = (uint16_t)frame[len - 2]
                          | ((uint16_t)frame[len - 1] << 8);
        if (crc_calc != crc_recv) {
            return EGW_RET_CODE(ERR_INVALID_ARG);
        }
        *unit_id_out = frame[0];
        *pdu_len_out = len - 3;
        memcpy(pdu_out, frame + 1, *pdu_len_out);
        return EGW_OK;
    }
    case EGW_MODBUS_TCP: {
        if (len < 8) {
            return EGW_RET_CODE(ERR_INVALID_ARG);
        }
        uint16_t mbap_len = ((uint16_t)frame[4] << 8) | frame[5];
        if (mbap_len < 1 || len < (size_t)(mbap_len + 6)) {
            return EGW_RET_CODE(ERR_INVALID_ARG);
        }
        *unit_id_out = frame[6];
        *pdu_len_out = mbap_len - 1;
        memcpy(pdu_out, frame + 7, *pdu_len_out);
        return EGW_OK;
    }
    }
    return EGW_RET_CODE(ERR_INVALID_ARG);
}

/* ── PDU 构建 ────────────────────────────────────────── */

static size_t build_pdu(uint8_t *pdu, uint8_t fc,
                         const uint8_t *payload, size_t payload_len)
{
    if (!pdu) {
        return 0;
    }
    pdu[0] = fc;
    if (payload_len > 0 && payload) {
        memcpy(pdu + 1, payload, payload_len);
    }
    return 1 + payload_len;
}

size_t egw_modbus_build_read_pdu(uint8_t *pdu, uint8_t fc,
                                  uint16_t addr, uint16_t count)
{
    if (!pdu || fc < 1 || fc > 4 || count == 0) {
        return 0;
    }

    uint8_t payload[4];
    payload[0] = (uint8_t)(addr >> 8);
    payload[1] = (uint8_t)(addr & 0xFF);
    payload[2] = (uint8_t)(count >> 8);
    payload[3] = (uint8_t)(count & 0xFF);
    return build_pdu(pdu, fc, payload, 4);
}

size_t egw_modbus_build_write_single_coil_pdu(uint8_t *pdu,
                                               uint16_t addr, uint16_t value)
{
    if (!pdu) {
        return 0;
    }
    uint8_t payload[4];
    payload[0] = (uint8_t)(addr >> 8);
    payload[1] = (uint8_t)(addr & 0xFF);
    payload[2] = (uint8_t)(value >> 8);
    payload[3] = (uint8_t)(value & 0xFF);
    return build_pdu(pdu, 0x05, payload, 4);
}

size_t egw_modbus_build_write_single_reg_pdu(uint8_t *pdu,
                                              uint16_t addr, uint16_t value)
{
    if (!pdu) {
        return 0;
    }
    uint8_t payload[4];
    payload[0] = (uint8_t)(addr >> 8);
    payload[1] = (uint8_t)(addr & 0xFF);
    payload[2] = (uint8_t)(value >> 8);
    payload[3] = (uint8_t)(value & 0xFF);
    return build_pdu(pdu, 0x06, payload, 4);
}

size_t egw_modbus_build_write_multiple_coils_pdu(uint8_t *pdu, uint16_t addr,
                                                  const uint8_t *values,
                                                  uint16_t count)
{
    if (!pdu || !values || count == 0 || count > 2000) {
        return 0;
    }

    uint8_t byte_count = (uint8_t)((count + 7) / 8);
    uint8_t payload[EGW_MODBUS_MAX_PDU];
    payload[0] = (uint8_t)(addr >> 8);
    payload[1] = (uint8_t)(addr & 0xFF);
    payload[2] = (uint8_t)(count >> 8);
    payload[3] = (uint8_t)(count & 0xFF);
    payload[4] = byte_count;
    memcpy(payload + 5, values, byte_count);
    return build_pdu(pdu, 0x0F, payload, 5 + byte_count);
}

size_t egw_modbus_build_write_multiple_regs_pdu(uint8_t *pdu, uint16_t addr,
                                                 const uint16_t *values,
                                                 uint16_t count)
{
    if (!pdu || !values || count == 0 || count > 123) {
        return 0;
    }

    uint8_t byte_count = (uint8_t)(count * 2);
    uint8_t payload[EGW_MODBUS_MAX_PDU];
    payload[0] = (uint8_t)(addr >> 8);
    payload[1] = (uint8_t)(addr & 0xFF);
    payload[2] = (uint8_t)(count >> 8);
    payload[3] = (uint8_t)(count & 0xFF);
    payload[4] = byte_count;
    for (uint16_t i = 0; i < count; i++) {
        payload[5 + i * 2]     = (uint8_t)(values[i] >> 8);
        payload[5 + i * 2 + 1] = (uint8_t)(values[i] & 0xFF);
    }
    return build_pdu(pdu, 0x10, payload, 5 + byte_count);
}

size_t egw_modbus_build_exception_pdu(uint8_t *pdu, uint8_t fc, uint8_t exc)
{
    if (!pdu) {
        return 0;
    }
    pdu[0] = (uint8_t)(fc | 0x80);
    pdu[1] = exc;
    return 2;
}

size_t egw_modbus_build_read_resp_pdu(uint8_t *pdu, uint8_t fc,
                                       const uint8_t *data, size_t data_len)
{
    if (!pdu) {
        return 0;
    }

    uint8_t byte_count = (uint8_t)data_len;
    pdu[0] = fc;
    pdu[1] = byte_count;
    memcpy(pdu + 2, data, byte_count);
    return 2 + byte_count;
}

/* ── PDU 解析 ────────────────────────────────────────── */

int egw_modbus_parse_read_pdu(const uint8_t *pdu, size_t len,
                               uint8_t fc,
                               uint16_t *regs, uint16_t max_regs)
{
    if (!pdu || !regs) {
        return -1;
    }

    if ((pdu[0] & 0x80) && (pdu[0] & 0x7F) == fc) {
        /* Modbus 异常响应：返回 -(exc + 100) 以避开通用错误 -1 */
        return -(int)pdu[1] - 100;
    }

    if (pdu[0] != fc) {
        return -1;
    }

    uint8_t byte_count = pdu[1];

    switch (fc) {
    case 0x01:
    case 0x02: {
        if ((size_t)(2 + byte_count) > len) {
            return -1;
        }
        if (byte_count > max_regs) {
            return -1;
        }
        for (uint16_t i = 0; i < byte_count; i++) {
            regs[i] = pdu[2 + i];
        }
        return (int)byte_count;
    }
    case 0x03:
    case 0x04: {
        if ((size_t)(2 + byte_count) > len) {
            return -1;
        }
        if (byte_count % 2 != 0 || byte_count / 2 > max_regs) {
            return -1;
        }
        uint16_t n = byte_count / 2;
        for (uint16_t i = 0; i < n; i++) {
            regs[i] = (uint16_t)pdu[2 + i * 2] << 8
                    | (uint16_t)pdu[2 + i * 2 + 1];
        }
        return (int)n;
    }
    }
    return -1;
}

egw_err_t egw_modbus_parse_write_pdu(const uint8_t *pdu, size_t len,
                                      uint8_t fc)
{
    if (!pdu || len < 5) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }
    if ((pdu[0] & 0x80) && (pdu[0] & 0x7F) == fc) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }
    if (pdu[0] != fc) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }
    return EGW_OK;
}

egw_err_t egw_modbus_parse_request(const uint8_t *pdu, size_t len,
                                    uint8_t *fc_out,
                                    uint16_t *addr_out, uint16_t *count_out,
                                    const uint8_t **data,
                                    size_t *data_len_out)
{
    if (!pdu || !fc_out || !addr_out || !count_out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    uint8_t fc = pdu[0];
    *fc_out = fc;

    switch (fc) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04: {
        if (len < 5) {
            return EGW_RET_CODE(ERR_INVALID_ARG);
        }
        *addr_out  = ((uint16_t)pdu[1] << 8) | pdu[2];
        *count_out = ((uint16_t)pdu[3] << 8) | pdu[4];
        if (data)        { *data = NULL; }
        if (data_len_out) { *data_len_out = 0; }
        return EGW_OK;
    }
    case 0x05:
    case 0x06: {
        if (len < 5) {
            return EGW_RET_CODE(ERR_INVALID_ARG);
        }
        *addr_out  = ((uint16_t)pdu[1] << 8) | pdu[2];
        *count_out = 1;
        if (data)        { *data = pdu + 3; }
        if (data_len_out) { *data_len_out = 2; }
        return EGW_OK;
    }
    case 0x0F:
    case 0x10: {
        if (len < 6) {
            return EGW_RET_CODE(ERR_INVALID_ARG);
        }
        *addr_out  = ((uint16_t)pdu[1] << 8) | pdu[2];
        *count_out = ((uint16_t)pdu[3] << 8) | pdu[4];
        uint8_t byte_count = pdu[5];
        if (data)        { *data = pdu + 6; }
        if (data_len_out) { *data_len_out = byte_count; }
        return EGW_OK;
    }
    }
    return EGW_RET_CODE(ERR_INVALID_ARG);
}

/* ── 类型转换 ────────────────────────────────────────── */

egw_err_t egw_modbus_regs_to_value(const uint16_t *regs, uint16_t count,
                                    egw_ctype_t ctype, egw_value_t *val)
{
    if (!regs || count == 0 || !val) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }
    memset(val, 0, sizeof(*val));

    switch (ctype) {
    case EGW_CTYPE_BOOL:
        val->b = (uint8_t)(regs[0] & 1);
        break;
    case EGW_CTYPE_U16:
        val->u16 = regs[0];
        break;
    case EGW_CTYPE_I16:
        val->i16 = (int16_t)regs[0];
        break;
    case EGW_CTYPE_U32: {
        uint32_t tmp = count >= 2
            ? ((uint32_t)regs[0] << 16) | regs[1]
            : (uint32_t)regs[0];
        val->u32 = tmp;
        break;
    }
    case EGW_CTYPE_I32: {
        uint32_t tmp = count >= 2
            ? ((uint32_t)regs[0] << 16) | regs[1]
            : (uint16_t)regs[0];
        val->i32 = (int32_t)tmp;
        break;
    }
    case EGW_CTYPE_F32: {
        uint32_t tmp = count >= 2
            ? ((uint32_t)regs[0] << 16) | regs[1]
            : 0;
        memcpy(&val->f32, &tmp, sizeof(tmp));
        break;
    }
    case EGW_CTYPE_U64:
    case EGW_CTYPE_I64: {
        uint64_t tmp = 0;
        if (count >= 4) {
            tmp = ((uint64_t)regs[0] << 48)
                | ((uint64_t)regs[1] << 32)
                | ((uint64_t)regs[2] << 16)
                |  regs[3];
        }
        if (ctype == EGW_CTYPE_U64) { val->u64 = tmp; }
        else                         { val->i64 = (int64_t)tmp; }
        break;
    }
    case EGW_CTYPE_F64: {
        uint64_t tmp = 0;
        if (count >= 4) {
            tmp = ((uint64_t)regs[0] << 48)
                | ((uint64_t)regs[1] << 32)
                | ((uint64_t)regs[2] << 16)
                |  regs[3];
        }
        memcpy(&val->f64, &tmp, sizeof(tmp));
        break;
    }
    default:
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }
    return EGW_OK;
}

/* ── Client（主站）状态机 ───────────────────────────── */

void egw_modbus_req_init(egw_modbus_req_t *req,
                          egw_modbus_transport_t transport,
                          uint8_t unit_id, uint8_t fc,
                          uint16_t addr, uint16_t count,
                          egw_ctype_t ctype,
                          uint32_t read_timeout_ms,
                          uint32_t sig_id,
                          egw_modbus_done_cb cb, void *arg)
{
    if (!req) {
        return;
    }

    memset(req, 0, sizeof(*req));

    req->transport       = transport;
    req->unit_id         = unit_id;
    req->fc              = fc;
    req->ctype           = ctype;
    req->read_timeout_ms = read_timeout_ms;
    req->sig_id          = sig_id;
    req->done_cb         = cb;
    req->done_arg        = arg;

    uint8_t pdu[EGW_MODBUS_MAX_PDU];
    size_t pdu_len = egw_modbus_build_read_pdu(pdu, fc, addr, count);
    if (pdu_len == 0) {
        req->state = EGW_MODBUS_ERROR;
        return;
    }

    req->len = egw_modbus_wrap_pdu(req->buf, transport, unit_id,
                                    pdu, pdu_len, 0);
    if (req->len == 0) {
        req->state = EGW_MODBUS_ERROR;
        return;
    }

    req->parser = egw_proto_modbus_open(&(egw_proto_modbus_params_t){
        .buf_size = EGW_MODBUS_MAX_FRAME,
        .dir      = EGW_PROTO_DIR_RESPONSE,
    });
    if (!req->parser) {
        req->state = EGW_MODBUS_ERROR;
        return;
    }

    req->state = EGW_MODBUS_SENDING;
}

void egw_modbus_req_destroy(egw_modbus_req_t *req)
{
    if (!req) {
        return;
    }
    if (req->parser) {
        egw_proto_close(req->parser);
        req->parser = NULL;
    }
}

void egw_modbus_req_send(egw_modbus_req_t *req, uint32_t now_ms)
{
    if (!req || req->state != EGW_MODBUS_SENDING) {
        return;
    }

    if (req->read_timeout_ms > 0) {
        req->deadline_ms = (int64_t)now_ms + req->read_timeout_ms;
    } else {
        req->deadline_ms = 0;
    }

    egw_proto_reset(req->parser);
    req->state = EGW_MODBUS_WAITING;
}

/* ── Client（主站）：帧已就绪后的解析 + 状态转移 ─────── */

static void req_handle_frame(egw_modbus_req_t *req)
{
    size_t frame_len = 0;
    const uint8_t *frame = egw_proto_get_frame(req->parser, &frame_len);

    uint8_t pdu[EGW_MODBUS_MAX_PDU];
    size_t pdu_len = 0;
    uint8_t unit_id = 0;

    if (egw_modbus_unwrap_frame(frame, frame_len, req->transport,
                                  &unit_id, pdu, &pdu_len) != EGW_OK) {
        req->state = EGW_MODBUS_ERROR;
        return;
    }

    if (unit_id != req->unit_id) {
        req->state = EGW_MODBUS_ERROR;
        return;
    }

    int n = egw_modbus_parse_read_pdu(pdu, pdu_len, req->fc,
                                       req->regs, 128);
    if (n < 0) {
        req->state = EGW_MODBUS_ERROR;
        return;
    }

    req->regs_parsed = n;
    if (egw_modbus_regs_to_value(req->regs, (uint16_t)n,
                                   req->ctype, &req->value) != EGW_OK) {
        req->state = EGW_MODBUS_ERROR;
        return;
    }

    req->state = EGW_MODBUS_DONE;
}

void egw_modbus_req_feed(egw_modbus_req_t *req,
                           const uint8_t *data, size_t len)
{
    if (!req || req->state != EGW_MODBUS_WAITING || !data || len == 0) {
        return;
    }

    egw_proto_result_t r = egw_proto_feed(req->parser, data, len);
    if (r != EGW_PROTO_FRAME_READY) {
        return;
    }

    req_handle_frame(req);
}

uint8_t *egw_modbus_req_reserve(egw_modbus_req_t *req, size_t *avail)
{
    if (!req || req->state != EGW_MODBUS_WAITING || !avail) {
        if (avail) {
            *avail = 0;
        }
        return NULL;
    }
    return egw_proto_reserve(req->parser, avail);
}

void egw_modbus_req_commit(egw_modbus_req_t *req, size_t n)
{
    if (!req || req->state != EGW_MODBUS_WAITING || n == 0) {
        return;
    }

    egw_proto_result_t r = egw_proto_commit(req->parser, n);
    if (r != EGW_PROTO_FRAME_READY) {
        return;
    }

    req_handle_frame(req);
}

egw_modbus_state_t egw_modbus_req_process(egw_modbus_req_t *req,
                                           uint32_t now_ms)
{
    if (!req) {
        return EGW_MODBUS_ERROR;
    }

    if (req->state == EGW_MODBUS_WAITING && req->deadline_ms > 0) {
        if ((int64_t)now_ms >= req->deadline_ms) {
            req->state = EGW_MODBUS_ERROR;
        }
    }

    if ((req->state == EGW_MODBUS_DONE || req->state == EGW_MODBUS_ERROR)
        && req->done_cb) {
        req->done_cb(req->unit_id, req->sig_id, req->value, req->done_arg);
        req->done_cb = NULL;
        return EGW_MODBUS_IDLE;
    }

    return req->state;
}

/* ── Server（从站）内部辅助 ──────────────────────────── */

struct egw_modbus_server {
    egw_modbus_transport_t  transport;
    uint8_t                 unit_id;
    egw_modbus_srv_read_cb  read_cb;
    egw_modbus_srv_write_cb write_cb;
    void                   *cb_arg;

    egw_proto_handle_t      *parser;

    bool                    has_response;
    uint8_t                 resp_buf[EGW_MODBUS_MAX_FRAME];
    size_t                  resp_len;
};

static egw_err_t handle_read_request(egw_modbus_server_t *s,
                                      uint8_t fc,
                                      uint16_t addr, uint16_t count,
                                      uint8_t *resp_pdu,
                                      size_t *resp_pdu_len)
{
    uint16_t regs[256];
    egw_err_t err = s->read_cb(addr, count, regs, s->unit_id, s->cb_arg);
    if (err != EGW_OK) {
        return err;
    }

    uint8_t raw[512];
    size_t raw_len = 0;

    if (fc == 0x01 || fc == 0x02) {
        raw_len = (size_t)((count + 7) / 8);
        for (uint16_t i = 0; i < raw_len; i++) {
            raw[i] = (uint8_t)regs[i];
        }
    } else {
        raw_len = (size_t)count * 2;
        for (uint16_t i = 0; i < count; i++) {
            raw[i * 2]     = (uint8_t)(regs[i] >> 8);
            raw[i * 2 + 1] = (uint8_t)(regs[i] & 0xFF);
        }
    }

    *resp_pdu_len = egw_modbus_build_read_resp_pdu(resp_pdu, fc,
                                                    raw, raw_len);
    return (*resp_pdu_len > 0) ? EGW_OK : EGW_RET_CODE(ERR_INVALID_ARG);
}

static egw_err_t handle_write_request(egw_modbus_server_t *s,
                                       uint8_t fc,
                                       uint16_t addr, uint16_t count,
                                       const uint8_t *req_data,
                                       const uint8_t *orig_pdu,
                                       size_t orig_pdu_len,
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
            for (uint16_t i = 0; i < nregs && i < 256; i++) {
                wregs[i] = 0;
            }
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
        egw_err_t err = s->write_cb(addr, nregs, wregs,
                                     s->unit_id, s->cb_arg);
        if (err != EGW_OK) {
            return err;
        }
    }

    memcpy(resp_pdu, orig_pdu, orig_pdu_len);
    *resp_pdu_len = orig_pdu_len;
    return EGW_OK;
}

/* ── Server（从站）状态机 ───────────────────────────── */

static void server_handle_frame(egw_modbus_server_t *s);

egw_modbus_server_t *egw_modbus_server_create(
    egw_modbus_transport_t transport,
    uint8_t unit_id,
    egw_modbus_srv_read_cb read_cb,
    egw_modbus_srv_write_cb write_cb,
    void *arg)
{
    egw_modbus_server_t *s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }

    s->transport = transport;
    s->unit_id   = unit_id;
    s->read_cb   = read_cb;
    s->write_cb  = write_cb;
    s->cb_arg    = arg;

    s->parser = egw_proto_modbus_open(&(egw_proto_modbus_params_t){
        .buf_size = EGW_MODBUS_MAX_FRAME,
        .dir      = EGW_PROTO_DIR_REQUEST,
    });
    if (!s->parser) {
        free(s);
        return NULL;
    }

    return s;
}

void egw_modbus_server_destroy(egw_modbus_server_t *s)
{
    if (!s) {
        return;
    }
    egw_proto_close(s->parser);
    free(s);
}

void egw_modbus_server_feed(egw_modbus_server_t *s,
                              const uint8_t *data, size_t len)
{
    if (!s || !data || len == 0 || s->has_response) {
        return;
    }

    egw_proto_result_t r = egw_proto_feed(s->parser, data, len);
    if (r != EGW_PROTO_FRAME_READY) {
        return;
    }

    server_handle_frame(s);
}

uint8_t *egw_modbus_server_reserve(egw_modbus_server_t *s, size_t *avail)
{
    if (!s || s->has_response || !avail) {
        if (avail) {
            *avail = 0;
        }
        return NULL;
    }
    return egw_proto_reserve(s->parser, avail);
}

void egw_modbus_server_commit(egw_modbus_server_t *s, size_t n)
{
    if (!s || n == 0 || s->has_response) {
        return;
    }

    egw_proto_result_t r = egw_proto_commit(s->parser, n);
    if (r != EGW_PROTO_FRAME_READY) {
        return;
    }

    server_handle_frame(s);
}

/* ── Server（从站）：帧已就绪后的处理 + 响应生成 ─────── */

static void server_handle_frame(egw_modbus_server_t *s)
{
    size_t frame_len = 0;
    const uint8_t *frame = egw_proto_get_frame(s->parser, &frame_len);

    uint8_t req_pdu[EGW_MODBUS_MAX_PDU];
    size_t  req_pdu_len = 0;
    uint8_t req_unit_id = 0;

    if (egw_modbus_unwrap_frame(frame, frame_len, s->transport,
                                 &req_unit_id, req_pdu,
                                 &req_pdu_len) != EGW_OK) {
        return;
    }

    if (req_unit_id != 0 && req_unit_id != s->unit_id) {
        return;
    }

    uint8_t      fc = 0;
    uint16_t     addr = 0, count = 0;
    const uint8_t *req_data = NULL;
    size_t        req_data_len = 0;

    if (egw_modbus_parse_request(req_pdu, req_pdu_len,
                                  &fc, &addr, &count,
                                  &req_data, &req_data_len) != EGW_OK) {
        uint8_t pdu[8];
        size_t plen = egw_modbus_build_exception_pdu(
            pdu, fc, EGW_MODBUS_EXC_ILLEGAL_DATA_ADDR);
        s->resp_len = egw_modbus_wrap_pdu(
            s->resp_buf, s->transport, s->unit_id, pdu, plen, 0);
        s->has_response = true;
        return;
    }

    uint8_t resp_pdu[EGW_MODBUS_MAX_PDU];
    size_t  resp_pdu_len = 0;

    switch (fc) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04: {
        if (handle_read_request(s, fc, addr, count,
                                 resp_pdu, &resp_pdu_len) != EGW_OK) {
            resp_pdu_len = egw_modbus_build_exception_pdu(
                resp_pdu, fc, EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE);
        }
        break;
    }
    case 0x05:
    case 0x06:
    case 0x0F:
    case 0x10: {
        if (handle_write_request(s, fc, addr, count,
                                  req_data, req_pdu, req_pdu_len,
                                  resp_pdu, &resp_pdu_len) != EGW_OK) {
            resp_pdu_len = egw_modbus_build_exception_pdu(
                resp_pdu, fc, EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE);
        }
        break;
    }
    default:
        resp_pdu_len = egw_modbus_build_exception_pdu(
            resp_pdu, fc, EGW_MODBUS_EXC_ILLEGAL_FUNCTION);
        break;
    }

    if (resp_pdu_len == 0) {
        return;
    }

    s->resp_len = egw_modbus_wrap_pdu(
        s->resp_buf, s->transport, s->unit_id,
        resp_pdu, resp_pdu_len, 0);
    s->has_response = (s->resp_len > 0);
}

bool egw_modbus_server_response_ready(const egw_modbus_server_t *s)
{
    return s ? s->has_response : false;
}

const uint8_t *egw_modbus_server_get_response(egw_modbus_server_t *s,
                                               size_t *len)
{
    if (!s || !len) {
        return NULL;
    }
    *len = s->resp_len;
    return s->resp_buf;
}

void egw_modbus_server_response_sent(egw_modbus_server_t *s)
{
    if (!s) {
        return;
    }
    s->has_response = false;
    s->resp_len = 0;
}

/* ── 点表字段表（供 ptable 注册时复用） ──────────────── */

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

const egw_field_t *egw_modbus_master_fields(size_t *count)
{
    if (count) {
        *count = sizeof(s_master_fields) / sizeof(s_master_fields[0]);
    }
    return s_master_fields;
}

const egw_field_t *egw_modbus_slave_fields(size_t *count)
{
    if (count) {
        *count = sizeof(s_slave_fields) / sizeof(s_slave_fields[0]);
    }
    return s_slave_fields;
}
