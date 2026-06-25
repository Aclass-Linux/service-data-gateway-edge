#ifndef EGW_MODBUS_SERVER_H
#define EGW_MODBUS_SERVER_H

#include "egw_modbus_transport.h"
#include <stdbool.h>

/* ── Server（从站） ──────────────────────────────────── */

typedef egw_err_t (*egw_modbus_srv_read_cb)(uint16_t address, uint16_t quantity,
                                              uint16_t *regs_out,
                                              uint8_t unit_id, void *arg);

typedef egw_err_t (*egw_modbus_srv_write_cb)(uint16_t address, uint16_t quantity,
                                               const uint16_t *regs,
                                               uint8_t unit_id, void *arg);

typedef struct egw_modbus_server egw_modbus_server_t;

/** @brief 从站创建参数 */
typedef struct {
    egw_modbus_transport_t  transport;  /* RTU / TCP */
    uint8_t                 unit_id;    /* 首个从站地址（0=跳过，之后 add_unit） */
    egw_modbus_srv_read_cb  read_cb;    /* 读寄存器回调 */
    egw_modbus_srv_write_cb write_cb;   /* 写寄存器回调（可为 NULL） */
    void                   *cb_arg;     /* 透传给回调 */
} egw_modbus_server_params_t;

/** @brief 创建从站实例（一个物理端口一个实例）
 *  @param params 创建参数，不可为 NULL
 *  @return 句柄，失败返回 NULL
 */
egw_modbus_server_t *egw_modbus_server_create(const egw_modbus_server_params_t *params);

/** @brief 注册一个从站地址（激活该 unit_id 的监听）
 *  @param s       从站句柄
 *  @param unit_id 从站地址（1-247，0=广播）
 *  @return EGW_OK 或错误码
 */
egw_err_t egw_modbus_server_add_unit(egw_modbus_server_t *s,
                                      uint8_t unit_id);

void egw_modbus_server_destroy(egw_modbus_server_t *s);

void egw_modbus_server_feed(egw_modbus_server_t *s,
                              const uint8_t *data, size_t len);

uint8_t *egw_modbus_server_reserve(egw_modbus_server_t *s, size_t *avail);

void egw_modbus_server_commit(egw_modbus_server_t *s, size_t n);

bool egw_modbus_server_response_ready(const egw_modbus_server_t *s);

const uint8_t *egw_modbus_server_get_response(egw_modbus_server_t *s,
                                               size_t *len);

/** @brief 标记响应已发送（清空内部状态，准备收下一帧） */
void egw_modbus_server_response_sent(egw_modbus_server_t *s);

#endif /* EGW_MODBUS_SERVER_H */
