#ifndef EGW_MODBUS_CLIENT_H
#define EGW_MODBUS_CLIENT_H

#include "egw_modbus_transport.h"

/* ── 请求 slot ──────────────────────────────────────── */

typedef struct egw_modbus_req_slot {
    struct egw_modbus_req_slot *next;
    uint8_t                    *buf;       /* 请求帧（一次组帧，永久复用） */
    size_t                     len;
    uint8_t                    unit_id;
    uint16_t                   addr;
} egw_modbus_req_slot_t;

/* ── Client（主站） ──────────────────────────────────── */

typedef void (*egw_modbus_done_cb)(uint8_t unit_id, uint16_t addr,
                                     const uint16_t *regs, int reg_count,
                                     void *arg);

typedef struct egw_modbus_client egw_modbus_client_t;

egw_modbus_client_t *egw_modbus_client_create(egw_modbus_transport_t transport,
                                                egw_modbus_done_cb done_cb,
                                                void *cb_arg);

void egw_modbus_client_destroy(egw_modbus_client_t *c);

/** @brief 注册一个 Modbus 读请求（一次组帧，永久复用）
 *  @param c        从站句柄
 *  @param unit_id  从站地址
 *  @param funccode  功能码（EGW_MODBUS_FC_READ_*）
 *  @param addr     寄存器起始地址
 *  @param count    寄存器数量
 *  @return slot 句柄，失败返回 NULL
 */
egw_modbus_req_slot_t *egw_modbus_client_register(egw_modbus_client_t *c,
                                                    uint8_t unit_id,
                                                    uint8_t funccode,
                                                    uint16_t addr,
                                                    uint16_t count);

/** @brief 获取请求帧 + 标记该 slot 为当前请求（准备收响应）
 *  @param c     从站句柄
 *  @param slot  slot 句柄
 *  @param len   输出帧长度
 *  @return 帧数据指针（失败返回 NULL）
 */
const uint8_t *egw_modbus_client_send(egw_modbus_client_t *c,
                                       egw_modbus_req_slot_t *slot,
                                       size_t *len);

void egw_modbus_client_feed(egw_modbus_client_t *c,
                              const uint8_t *data, size_t len);

uint8_t *egw_modbus_client_reserve(egw_modbus_client_t *c, size_t *avail);

void egw_modbus_client_commit(egw_modbus_client_t *c, size_t n);

#endif /* EGW_MODBUS_CLIENT_H */
