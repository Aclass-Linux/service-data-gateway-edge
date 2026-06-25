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
