#include "egw_modbus_core.h"
#include "egw_crc.h"
#include <string.h>
#include <stdlib.h>

/* ── 构建读响应 PDU（服务器用） ──────────────────────── */

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

/* ── 帧封装／解封装（输运层） ──────────────────────────── */

static egw_err_t egw_modbus_decode_pdu(const uint8_t *frame, size_t len,
                                       egw_modbus_transport_t transport,
                                       uint8_t *unit_id_out,
                                       uint8_t *pdu_out, size_t *pdu_len_out)
{
    if (!frame || !unit_id_out || !pdu_out || !pdu_len_out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    switch (transport) {
    case EGW_MODBUS_RTU: {
        if (len < 4) { return EGW_RET_CODE(ERR_INVALID_ARG); }
        uint16_t crc_calc = egw_crc_modbus_table(frame, len - 2);
        uint16_t crc_recv = (uint16_t)frame[len - 2]
                          | ((uint16_t)frame[len - 1] << 8);
        if (crc_calc != crc_recv) { return EGW_RET_CODE(ERR_INVALID_ARG); }
        *unit_id_out = frame[0];
        *pdu_len_out = len - 3;
        memcpy(pdu_out, frame + 1, *pdu_len_out);
        return EGW_OK;
    }
    case EGW_MODBUS_TCP: {
        if (len < 8) { return EGW_RET_CODE(ERR_INVALID_ARG); }
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

/* ── write_pdu: 算长度 → 分配/借用 → 写 PDU ──────────── */

static uint8_t* alloc_buf(size_t *out_len, uint8_t *buf, size_t cap,
                           size_t adu_len)
{
    if (buf) {
        if (adu_len > cap) { *out_len = 0; return NULL; }
        memset(buf, 0, adu_len);
        *out_len = adu_len;
        return buf;
    }
    buf = (uint8_t *)calloc(1, adu_len);
    if (!buf) { *out_len = 0; return NULL; }
    *out_len = adu_len;
    return buf;
}

static uint8_t* write_pdu(size_t *out_len,
                           const egw_modbus_encode_params_t *params,
                           egw_modbus_transport_t transport)
{
    size_t pdu_off = (transport == EGW_MODBUS_RTU) ? 1 : 7;
    size_t adu_len;
    uint8_t *buf;
    uint8_t byte_cnt;
    switch (params->type) {
    case EGW_ENCODE_FROM_PARAMS:
        switch (params->funccode) {
        case 0x01: case 0x02: case 0x03: case 0x04:
            adu_len = pdu_off + 5 + ((transport == EGW_MODBUS_RTU) ? 2 : 0);
            buf = alloc_buf(out_len, params->buf, params->cap, adu_len);
            if (!buf) { *out_len = 0; return NULL; }
            buf[pdu_off]     = params->funccode;
            buf[pdu_off + 1] = (uint8_t)(params->addr >> 8);
            buf[pdu_off + 2] = (uint8_t)(params->addr & 0xFF);
            buf[pdu_off + 3] = (uint8_t)(params->count >> 8);
            buf[pdu_off + 4] = (uint8_t)(params->count & 0xFF);
            return buf;
        case 0x05:
            adu_len = pdu_off + 5 + ((transport == EGW_MODBUS_RTU) ? 2 : 0);
            buf = alloc_buf(out_len, params->buf, params->cap, adu_len);
            if (!buf) { *out_len = 0; return NULL; }
            buf[pdu_off]     = 0x05;
            buf[pdu_off + 1] = (uint8_t)(params->addr >> 8);
            buf[pdu_off + 2] = (uint8_t)(params->addr & 0xFF);
            buf[pdu_off + 3] = params->data ? ((const uint8_t *)params->data)[0] : 0;
            buf[pdu_off + 4] = params->data ? ((const uint8_t *)params->data)[1] : 0;
            return buf;
        case 0x06:
            adu_len = pdu_off + 5 + ((transport == EGW_MODBUS_RTU) ? 2 : 0);
            buf = alloc_buf(out_len, params->buf, params->cap, adu_len);
            if (!buf) { *out_len = 0; return NULL; }
            buf[pdu_off]     = 0x06;
            buf[pdu_off + 1] = (uint8_t)(params->addr >> 8);
            buf[pdu_off + 2] = (uint8_t)(params->addr & 0xFF);
            buf[pdu_off + 3] = params->data ? ((const uint8_t *)params->data)[0] : 0;
            buf[pdu_off + 4] = params->data ? ((const uint8_t *)params->data)[1] : 0;
            return buf;
        case 0x0F:
            byte_cnt = (uint8_t)((params->count + 7) / 8);
            adu_len = pdu_off + (size_t)5 + byte_cnt
                    + ((transport == EGW_MODBUS_RTU) ? 2 : 0);
            buf = alloc_buf(out_len, params->buf, params->cap, adu_len);
            if (!buf) { *out_len = 0; return NULL; }
            buf[pdu_off]     = 0x0F;
            buf[pdu_off + 1] = (uint8_t)(params->addr >> 8);
            buf[pdu_off + 2] = (uint8_t)(params->addr & 0xFF);
            buf[pdu_off + 3] = (uint8_t)(params->count >> 8);
            buf[pdu_off + 4] = (uint8_t)(params->count & 0xFF);
            buf[pdu_off + 5] = byte_cnt;
            memcpy(buf + pdu_off + 6, params->data, byte_cnt);
            return buf;
        case 0x10:
            byte_cnt = (uint8_t)(params->count * 2);
            adu_len = pdu_off + (size_t)5 + byte_cnt
                    + ((transport == EGW_MODBUS_RTU) ? 2 : 0);
            buf = alloc_buf(out_len, params->buf, params->cap, adu_len);
            if (!buf) { *out_len = 0; return NULL; }
            buf[pdu_off]     = 0x10;
            buf[pdu_off + 1] = (uint8_t)(params->addr >> 8);
            buf[pdu_off + 2] = (uint8_t)(params->addr & 0xFF);
            buf[pdu_off + 3] = (uint8_t)(params->count >> 8);
            buf[pdu_off + 4] = (uint8_t)(params->count & 0xFF);
            buf[pdu_off + 5] = byte_cnt;
            memcpy(buf + pdu_off + 6, params->data, byte_cnt);
            return buf;
        }
        *out_len = 0; return NULL;

    case EGW_ENCODE_PDU:
        adu_len = pdu_off + params->pdu_len
                + ((transport == EGW_MODBUS_RTU) ? 2 : 0);
        buf = alloc_buf(out_len, params->buf, params->cap, adu_len);
        if (!buf) { *out_len = 0; return NULL; }
        memcpy(buf + pdu_off, params->pdu, params->pdu_len);
        return buf;

    case EGW_ENCODE_EXCEPTION:
        adu_len = pdu_off + 2 + ((transport == EGW_MODBUS_RTU) ? 2 : 0);
        buf = alloc_buf(out_len, params->buf, params->cap, adu_len);
        if (!buf) { *out_len = 0; return NULL; }
        buf[pdu_off]     = (uint8_t)(params->funccode | 0x80);
        buf[pdu_off + 1] = params->exc_code;
        return buf;
    }
    *out_len = 0; return NULL;
}

/* ── egw_modbus_encode: 写帧头 + CRC ────────────────── */

uint8_t* egw_modbus_encode(egw_modbus_transport_t transport,
                            const egw_modbus_encode_params_t *params,
                            size_t *out_len)
{
    if (!params || !out_len) { return NULL; }

    uint8_t *buf = write_pdu(out_len, params, transport);
    if (!buf || *out_len == 0) { return NULL; }

    if (transport == EGW_MODBUS_RTU) {
        buf[0] = params->unit_id;
        uint16_t crc = egw_crc_modbus_table(buf, *out_len - 2);
        buf[*out_len - 2] = (uint8_t)(crc & 0xFF);
        buf[*out_len - 1] = (uint8_t)(crc >> 8);
    } else {
        uint16_t mbap_len = (uint16_t)(*out_len - 6);
        buf[0] = (uint8_t)(params->tid >> 8);
        buf[1] = (uint8_t)(params->tid & 0xFF);
        buf[2] = 0; buf[3] = 0;
        buf[4] = (uint8_t)(mbap_len >> 8);
        buf[5] = (uint8_t)(mbap_len & 0xFF);
        buf[6] = params->unit_id;
    }

    return buf;
}

egw_err_t egw_modbus_decode(egw_modbus_transport_t transport,
                             const uint8_t *frame, size_t len,
                             uint8_t *unit_id_out,
                             uint8_t *pdu_out, size_t *pdu_len_out)
{
    return egw_modbus_decode_pdu(frame, len, transport,
                                    unit_id_out, pdu_out, pdu_len_out);
}

