/**
 * @file egw_modbus_slave.h
 * @brief Modbus 北向服务 —— 点表结构 + 回环测试从站（Server）侧
 */

#ifndef EGW_MODBUS_SLAVE_H
#define EGW_MODBUS_SLAVE_H

#include "egw_modbus_master.h"

/* ── 北向服务标志位 ──────────────────────────────────── */

#define EGW_MODBUS_SLAVE_ENABLED           (1u << 0)
#define EGW_MODBUS_SLAVE_HAS_SCALE_OFFSET  (1u << 1)
#define EGW_MODBUS_SLAVE_HAS_DEADBAND      (1u << 2)

/* ── 北向服务配置行 ──────────────────────────────────── */

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
    uint16_t value;              /* 运行时寄存器值（name=NULL 不从 SQLite 加载） */
} egw_modbus_slave_t;

/** @brief 北向服务查找键（二分查找用） */
typedef struct {
    uint16_t unit_id;
    uint16_t reg_addr;
} egw_modbus_slave_key_t;

egw_ptable_rs_t *egw_modbus_slave_load(egw_ptable_t *pt);

/* ── 回环从站 API ────────────────────────────────────── */

egw_err_t egw_lb_slave_transport_open(egw_lb_ctx_t *ctx, const char *path);
egw_err_t egw_lb_slave_init(egw_lb_ctx_t *ctx, egw_ptable_rs_t *rs);
void      egw_lb_slave_poll_start(egw_lb_ctx_t *ctx);
void      egw_lb_slave_on_poll(uv_poll_t *p, int status, int events);
void      egw_lb_slave_cleanup(egw_lb_ctx_t *ctx);

#endif /* EGW_MODBUS_SLAVE_H */
