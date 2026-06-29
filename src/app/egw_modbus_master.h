/**
 * @file egw_modbus_master.h
 * @brief Modbus 南向采集 —— 点表结构 + 回环测试主站（Client）侧
 */

#ifndef EGW_MODBUS_MASTER_H
#define EGW_MODBUS_MASTER_H

#include "egw_defs.h"
#include "egw_ptable.h"
#include "egw_modbus_client.h"
#include "egw_modbus_server.h"
#include "egw_transport.h"
#include <stdint.h>
#include <uv.h>

/* ── 南向采集标志位 ──────────────────────────────────── */

#define EGW_MODBUS_MASTER_ENABLED           (1u << 0)
#define EGW_MODBUS_MASTER_HAS_SCALE_OFFSET  (1u << 1)
#define EGW_MODBUS_MASTER_HAS_DEADBAND      (1u << 2)

/* ── 南向采集配置行 ──────────────────────────────────── */

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
    uint16_t value;              /* 运行时寄存器值（name=NULL 不从 SQLite 加载） */
} egw_modbus_master_t;

/** @brief 南向采集查找键（二分查找用） */
typedef struct {
    uint16_t unit_id;
    uint16_t reg_addr;
} egw_modbus_master_key_t;

egw_ptable_rs_t *egw_modbus_master_load(egw_ptable_t *pt);

/* ── 回环测试共享上下文 ──────────────────────────────── */

typedef enum {
    EGW_LB_PHASE_SERVER_RECV,
    EGW_LB_PHASE_CLIENT_RECV,
    EGW_LB_PHASE_DONE,
} egw_lb_phase_t;

typedef struct {
    uv_loop_t              *loop;
    egw_transport_handle_t *cli_h;
    egw_transport_handle_t *srv_h;
    egw_modbus_server_t    *server;
    egw_modbus_client_t    *cli;
    egw_modbus_req_slot_t  *req_slot;
    egw_lb_phase_t          phase;
    bool                    seg_pending;
    uv_poll_t               cli_poll;
    uv_poll_t               srv_poll;
} egw_lb_ctx_t;

/* ── 回环主站 API ────────────────────────────────────── */

egw_err_t egw_lb_master_transport_open(egw_lb_ctx_t *ctx, const char *path);
egw_err_t egw_lb_master_init(egw_lb_ctx_t *ctx, egw_ptable_rs_t *rs);
egw_err_t egw_lb_master_send(egw_lb_ctx_t *ctx);
void      egw_lb_master_poll_init(egw_lb_ctx_t *ctx);
void      egw_lb_master_on_poll(uv_poll_t *p, int status, int events);
void      egw_lb_master_cleanup(egw_lb_ctx_t *ctx);
void      egw_lb_on_timeout(uv_timer_t *t);

#endif /* EGW_MODBUS_MASTER_H */
