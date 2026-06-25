#include "egw_modbus_core.h"
#include "egw_crc.h"
#include <string.h>
#include <stdlib.h>

/* ── PDU 构建── ──────────────────────────────────────── */

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

size_t egw_modbus_wrap_pdu(uint8_t *buf, egw_modbus_transport_t transport,
                            uint8_t unit_id,
                            const uint8_t *pdu, size_t pdu_len,
                            uint16_t tid)
{
    if (!buf || !pdu || pdu_len == 0) { return 0; }

    switch (transport) {
    case EGW_MODBUS_RTU: {
        if (pdu_len + 3 > EGW_MODBUS_MAX_FRAME) { return 0; }
        buf[0] = unit_id;
        memcpy(buf + 1, pdu, pdu_len);
        uint16_t crc = egw_crc_modbus_table(buf, pdu_len + 1);
        buf[pdu_len + 1] = (uint8_t)(crc & 0xFF);
        buf[pdu_len + 2] = (uint8_t)(crc >> 8);
        return pdu_len + 3;
    }
    case EGW_MODBUS_TCP: {
        if (pdu_len + 7 > EGW_MODBUS_MAX_FRAME) { return 0; }
        uint16_t mbap_len = (uint16_t)(pdu_len + 1);
        buf[0] = (uint8_t)(tid >> 8);
        buf[1] = (uint8_t)(tid & 0xFF);
        buf[2] = 0; buf[3] = 0;
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
/* ── 输运打包/解包适配器 ──────────────────────────── */

size_t egw_modbus_wrap_rtu(uint8_t *buf, uint8_t unit_id,
                            const uint8_t *pdu, size_t pdu_len)
{
    return egw_modbus_wrap_pdu(buf, EGW_MODBUS_RTU, unit_id, pdu, pdu_len, 0);
}

size_t egw_modbus_wrap_tcp(uint8_t *buf, uint8_t unit_id,
                            const uint8_t *pdu, size_t pdu_len)
{
    return egw_modbus_wrap_pdu(buf, EGW_MODBUS_TCP, unit_id, pdu, pdu_len, 0);
}

egw_err_t egw_modbus_unwrap_rtu(const uint8_t *frame, size_t len,
                                 uint8_t *unit_id_out,
                                 uint8_t *pdu_out, size_t *pdu_len_out)
{
    return egw_modbus_unwrap_frame(frame, len, EGW_MODBUS_RTU,
                                    unit_id_out, pdu_out, pdu_len_out);
}

egw_err_t egw_modbus_unwrap_tcp(const uint8_t *frame, size_t len,
                                 uint8_t *unit_id_out,
                                 uint8_t *pdu_out, size_t *pdu_len_out)
{
    return egw_modbus_unwrap_frame(frame, len, EGW_MODBUS_TCP,
                                    unit_id_out, pdu_out, pdu_len_out);
}
