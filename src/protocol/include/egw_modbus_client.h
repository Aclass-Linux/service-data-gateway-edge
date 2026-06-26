#ifndef EGW_MODBUS_CLIENT_H
#define EGW_MODBUS_CLIENT_H

#include "egw_modbus_core.h"

/* ── 请求 slot（不透明句柄） ─────────────────────────── */

typedef struct egw_modbus_req_slot egw_modbus_req_slot_t;

/* ── Client（主站） ──────────────────────────────────── */

typedef struct egw_modbus_client egw_modbus_client_t;

/** @brief 主站创建参数 */
typedef struct {
    egw_modbus_transport_t  transport;   /**< RTU 或 TCP */
    size_t                  recv_cap;    /**< 接收缓冲区容量（0=默认 EGW_MODBUS_MAX_FRAME） */
    egw_modbus_done_cb      done_cb;     /**< 读完成回调 */
    void                   *cb_arg;      /**< 回调用户参数 */
} egw_modbus_client_params_t;

/** @brief 创建主站实例 */
egw_modbus_client_t *egw_modbus_client_create(const egw_modbus_client_params_t *params);

/** @brief 销毁主站实例（释放链表全部 slot 及缓冲区） */
void egw_modbus_client_destroy(egw_modbus_client_t *client);

/* ── 请求管理 ─────────────────────────────────────────── */

/** @brief 注册请求，追加到环形链表尾部
 *  @return slot 指针，供 request / remove 使用 */
egw_modbus_req_slot_t *egw_modbus_client_register(egw_modbus_client_t *client,
                                                    const egw_modbus_encode_params_t *params);

/** @brief 从环形链表移除并释放一个 slot */
void egw_modbus_client_remove(egw_modbus_client_t *client,
                               egw_modbus_req_slot_t *slot);

/** @brief 发起指定 slot 的请求
 *
 * 设 current = slot，清接收缓冲区。
 * 返回帧指针供 transport write。
 *
 *  @param slot  由 register 返回的 slot 指针
 *  @param len   输出帧长度
 *  @return 帧数据指针
 */
const uint8_t *egw_modbus_client_request(egw_modbus_client_t *client,
                                          egw_modbus_req_slot_t *slot,
                                          size_t *len);

/* ── 数据接收 ─────────────────────────────────────────── */

void egw_modbus_client_feed(egw_modbus_client_t *client,
                              const uint8_t *data, size_t len);

uint8_t *egw_modbus_client_reserve(egw_modbus_client_t *client, size_t *avail);

void egw_modbus_client_commit(egw_modbus_client_t *client, size_t nbytes);

#endif /* EGW_MODBUS_CLIENT_H */
