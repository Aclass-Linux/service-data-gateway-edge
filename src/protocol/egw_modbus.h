/**
 * @file egw_modbus.h
 * @brief Modbus 点表数据结构（master / slave）
 *
 * master = 南向（采集端），slave = 北向（被采集端）。
 * 结构体配合 egw_ptable_register() 使用，字段映射表存于此。
 */

#ifndef EGW_MODBUS_H
#define EGW_MODBUS_H

#include "egw_defs.h"

/* ── Master（南向） ─────────────────────────────────── */

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

/* ── Slave（北向） ──────────────────────────────────── */

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

/* ── 路由表条目 ────────────────────────────────────── */

typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
    uint8_t  ctype;
} egw_route_entry_t;

#endif /* EGW_MODBUS_H */
