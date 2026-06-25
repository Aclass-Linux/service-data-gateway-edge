#ifndef EGW_MODBUS_CORE_H
#define EGW_MODBUS_CORE_H

#include "egw_defs.h"
#include <stdint.h>
#include <stddef.h>

/* ── 帧解析方向 ─────────────────────────────────────── */

typedef enum {
    EGW_PROTO_DIR_RESPONSE = 0,
    EGW_PROTO_DIR_REQUEST  = 1,
} egw_proto_dir_t;

/* ── 帧定界结果 ─────────────────────────────────────── */

typedef enum {
    EGW_PROTO_NEED_MORE,
    EGW_PROTO_FRAME_READY,
    EGW_PROTO_FRAME_ERROR,
} egw_proto_result_t;

/* ── Modbus 协议常量 ────────────────────────────────── */

#define EGW_MODBUS_MAX_PDU      253
#define EGW_MODBUS_MAX_FRAME    260

/* ── 功能码 ─────────────────────────────────────────── */

#define EGW_MODBUS_FC_READ_COILS               0x01
#define EGW_MODBUS_FC_READ_DISCRETE_INPUTS     0x02
#define EGW_MODBUS_FC_READ_HOLDING_REGISTERS   0x03
#define EGW_MODBUS_FC_READ_INPUT_REGISTERS     0x04
#define EGW_MODBUS_FC_WRITE_SINGLE_COIL        0x05
#define EGW_MODBUS_FC_WRITE_SINGLE_REGISTER    0x06
#define EGW_MODBUS_FC_WRITE_MULTIPLE_COILS     0x0F
#define EGW_MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10

#define EGW_MODBUS_EXC_ILLEGAL_FUNCTION      1
#define EGW_MODBUS_EXC_ILLEGAL_DATA_ADDR     2
#define EGW_MODBUS_EXC_ILLEGAL_DATA_VAL      3
#define EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE  4
#define EGW_MODBUS_EXC_ACKNOWLEDGE           5
#define EGW_MODBUS_EXC_SLAVE_DEVICE_BUSY     6
#define EGW_MODBUS_EXC_NEGATIVE_ACK          7
#define EGW_MODBUS_EXC_MEMORY_PARITY_ERROR   8
#define EGW_MODBUS_EXC_GATEWAY_PATH         10
#define EGW_MODBUS_EXC_GATEWAY_TARGET       11

/* ── 传输层类型 ─────────────────────────────────────── */

typedef enum {
    EGW_MODBUS_RTU = 1,
    EGW_MODBUS_TCP = 2,
} egw_modbus_transport_t;

/* ── 点表配置结构体 ──────────────────────────────────── */

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

/* ── 点表字段访问器 ──────────────────────────────────── */

const egw_field_t *egw_modbus_master_fields(size_t *count);
const egw_field_t *egw_modbus_slave_fields(size_t *count);

/* ── PDU 构建（传输无关） ────────────────────────────── */

size_t egw_modbus_build_read_pdu(uint8_t *pdu, uint8_t fc,
                                  uint16_t addr, uint16_t count);

size_t egw_modbus_build_write_single_coil_pdu(uint8_t *pdu,
                                               uint16_t addr, uint16_t value);

size_t egw_modbus_build_write_single_reg_pdu(uint8_t *pdu,
                                              uint16_t addr, uint16_t value);

size_t egw_modbus_build_write_multiple_coils_pdu(uint8_t *pdu, uint16_t addr,
                                                  const uint8_t *values,
                                                  uint16_t count);

size_t egw_modbus_build_write_multiple_regs_pdu(uint8_t *pdu, uint16_t addr,
                                                 const uint16_t *values,
                                                 uint16_t count);

size_t egw_modbus_build_exception_pdu(uint8_t *pdu, uint8_t fc, uint8_t exc);

size_t egw_modbus_build_read_resp_pdu(uint8_t *pdu, uint8_t fc,
                                       const uint8_t *data, size_t data_len);

/* ── PDU 解析（传输无关） ────────────────────────────── */

int egw_modbus_parse_read_pdu(const uint8_t *pdu, size_t len,
                               uint8_t fc,
                               uint16_t *regs, uint16_t max_regs);

egw_err_t egw_modbus_parse_write_pdu(const uint8_t *pdu, size_t len,
                                      uint8_t fc);

egw_err_t egw_modbus_parse_request(const uint8_t *pdu, size_t len,
                                    uint8_t *fc_out,
                                    uint16_t *addr_out, uint16_t *count_out,
                                    const uint8_t **data,
                                    size_t *data_len_out);

/* ── 类型转换 ────────────────────────────────────────── */

egw_err_t egw_modbus_regs_to_value(const uint16_t *regs, uint16_t count,
                                    egw_ctype_t ctype, egw_value_t *val);

#endif /* EGW_MODBUS_CORE_H */
