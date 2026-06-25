#include "egw_modbus_transport.h"
#include "egw_crc.h"
#include <string.h>

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

/* ── Server 输运适配器 ────────────────────────────────── */

size_t egw_modbus_ser_wrap_rtu(uint8_t *buf, uint8_t unit_id,
                                const uint8_t *pdu, size_t pdu_len)
{
    return egw_modbus_wrap_pdu(buf, EGW_MODBUS_RTU, unit_id, pdu, pdu_len, 0);
}

size_t egw_modbus_ser_wrap_tcp(uint8_t *buf, uint8_t unit_id,
                                const uint8_t *pdu, size_t pdu_len)
{
    return egw_modbus_wrap_pdu(buf, EGW_MODBUS_TCP, unit_id, pdu, pdu_len, 0);
}

egw_err_t egw_modbus_ser_unwrap_rtu(const uint8_t *frame, size_t len,
                                     uint8_t *unit_id_out,
                                     uint8_t *pdu_out, size_t *pdu_len_out)
{
    return egw_modbus_unwrap_frame(frame, len, EGW_MODBUS_RTU,
                                    unit_id_out, pdu_out, pdu_len_out);
}

egw_err_t egw_modbus_ser_unwrap_tcp(const uint8_t *frame, size_t len,
                                     uint8_t *unit_id_out,
                                     uint8_t *pdu_out, size_t *pdu_len_out)
{
    return egw_modbus_unwrap_frame(frame, len, EGW_MODBUS_TCP,
                                    unit_id_out, pdu_out, pdu_len_out);
}
