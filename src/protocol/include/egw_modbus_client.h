#ifndef EGW_MODBUS_CLIENT_H
#define EGW_MODBUS_CLIENT_H

#include "egw_modbus_core.h"

/* ── 请求 slot（不透明句柄） ─────────────────────────── */

typedef struct egw_modbus_req_slot egw_modbus_req_slot_t;

/* ── Client（主站） ──────────────────────────────────── */

/** @brief Modbus 读完成回调
 *  @param unit_id   从站地址
 *  @param addr      寄存器地址
 *  @param regs      原始寄存器值数组（CPU 字节序）
 *  @param reg_count 寄存器数量（< 0 表示错误）
 *  @param arg       用户参数
 */
typedef void (*egw_modbus_done_cb)(uint8_t unit_id, uint16_t addr,
                                     const uint16_t *regs, int reg_count,
                                     void *arg);

/** @brief 主站句柄（不透明，内部持有请求链表 + 接收缓冲区） */
typedef struct egw_modbus_client egw_modbus_client_t;

/** @brief 创建主站实例
 *  @param transport RTU 或 TCP
 *  @param done_cb  读完成回调
 *  @param cb_arg   回调用户参数
 *  @return 句柄，失败返回 NULL
 */
egw_modbus_client_t *egw_modbus_client_create(egw_modbus_transport_t transport,
                                                egw_modbus_done_cb done_cb,
                                                void *cb_arg);

/** @brief 销毁主站实例
 *  释放内部所有 slot 的帧缓冲区及接收缓冲区。 */
void egw_modbus_client_destroy(egw_modbus_client_t *c);

/** @brief 注册一个 Modbus 读请求的参数字段 */
typedef struct {
    uint8_t  unit_id;    /**< 从站地址 */
    uint8_t  funccode;   /**< 功能码（EGW_MODBUS_FC_READ_*） */
    uint16_t addr;       /**< 寄存器起始地址 */
    uint16_t count;      /**< 寄存器数量 */
} egw_modbus_req_params_t;

/** @brief 注册一个 Modbus 读请求
 *
 * 构建 PDU → 打包为完整 ADU（CRC/MBAP），结果存入 slot。
 * 一次注册，之后反复发送不再重建帧。
 *
 *  @param c      主站句柄
 *  @param params 请求参数
 *  @return slot 句柄，失败返回 NULL
 */
egw_modbus_req_slot_t *egw_modbus_client_register(egw_modbus_client_t *c,
                                                    const egw_modbus_req_params_t *params);

/** @brief 获取请求帧并标记该 slot 为当前
 *
 * 返回 slot 的请求帧指针 + 长度，同时清除接收缓冲区，
 * 准备接收该 slot 对应的响应。
 *
 *  @param c     主站句柄
 *  @param slot  slot 句柄
 *  @param len   输出帧长度
 *  @return 帧数据指针（指向 slot 内部缓冲区，destroy 前有效）
 */
const uint8_t *egw_modbus_client_send(egw_modbus_client_t *c,
                                       egw_modbus_req_slot_t *slot,
                                       size_t *len);

/** @brief 喂入响应字节（memcpy 路径）
 *
 * 数据拷贝到内部接收缓冲区，尝试帧定界。
 * 收到完整帧后自动解包→解析寄存器→调 done_cb。
 * 帧错误时清空接收缓冲区。 */
void egw_modbus_client_feed(egw_modbus_client_t *c,
                              const uint8_t *data, size_t len);

/** @brief 获取可写接收缓冲区（零拷贝路径）
 *
 * 返回内部接收缓冲区的空闲部分指针，
 * 调用方直接通过 transport read 写入，省一次 memcpy。
 * 写入后需调用 egw_modbus_client_commit 提交。
 *
 *  @param c     主站句柄
 *  @param avail 输出可写字节数
 *  @return 可写指针，失败返回 NULL
 */
uint8_t *egw_modbus_client_reserve(egw_modbus_client_t *c, size_t *avail);

/** @brief 提交已写入的接收字节
 *
 * 将 reserve 写入的字节数提交给解析器，
 * 触发帧定界 + 校验 + 回调。 */
void egw_modbus_client_commit(egw_modbus_client_t *c, size_t n);

#endif /* EGW_MODBUS_CLIENT_H */
