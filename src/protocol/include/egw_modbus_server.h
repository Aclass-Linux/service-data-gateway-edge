#ifndef EGW_MODBUS_SERVER_H
#define EGW_MODBUS_SERVER_H

#include "egw_modbus_core.h"

/* ── Server（从站） ──────────────────────────────────── */

/** @brief 读寄存器回调（FC01-04）
 *  @param address  寄存器地址
 *  @param quantity 数量
 *  @param regs_out 输出缓冲区（CPU 字节序，引擎回填）
 *  @param unit_id  请求的从站地址
 *  @param arg      用户参数
 *  @return EGW_OK 或错误码（错误时引擎自动发异常响应）
 */
typedef egw_err_t (*egw_modbus_srv_read_cb)(uint16_t address, uint16_t quantity,
                                              uint16_t *regs_out,
                                              uint8_t unit_id, void *arg);

/** @brief 写寄存器回调（FC05/06/0F/10）
 *  @param address  寄存器地址
 *  @param quantity 数量
 *  @param regs     寄存器值（CPU 字节序）
 *  @param unit_id  请求的从站地址
 *  @param arg      用户参数
 *  @return EGW_OK 或错误码
 */
typedef egw_err_t (*egw_modbus_srv_write_cb)(uint16_t address, uint16_t quantity,
                                               const uint16_t *regs,
                                               uint8_t unit_id, void *arg);

/** @brief 从站句柄（不透明，内部持有环形接收缓冲区 + unit 位图） */
typedef struct egw_modbus_server egw_modbus_server_t;

/** @brief 从站创建参数 */
typedef struct {
    egw_modbus_transport_t  transport;   /**< RTU 或 TCP */
    uint8_t                 unit_ids[4]; /**< 从站地址列表（0 结尾，之后 add_unit） */
    size_t                  buf_cap;     /**< 接收缓冲区容量（0=默认 EGW_MODBUS_MAX_FRAME） */
    size_t                  resp_cap;    /**< 响应缓冲区容量（0=默认 EGW_MODBUS_MAX_FRAME） */
    egw_modbus_srv_read_cb  read_cb;     /**< 读寄存器回调 */
    egw_modbus_srv_write_cb write_cb;    /**< 写寄存器回调（可为 NULL） */
    void                   *cb_arg;      /**< 透传给回调 */
} egw_modbus_server_params_t;

/** @brief 创建从站实例
 *
 * 一个物理端口对应一个实例。内部映射 transport → encode/unwrap 函数指针，
 * 可通过 params.unit_id 直接注册第一个从站地址。
 *
 *  @param params 创建参数，不可为 NULL
 *  @return 句柄，失败返回 NULL
 */
egw_modbus_server_t *egw_modbus_server_create(const egw_modbus_server_params_t *params);

/** @brief 注册一个从站地址
 *
 * 激活 unit_id 的监听。收到帧后匹配 unit 位图，
 * 命中则调对应回调，未命中直接丢弃。
 *
 *  @param s       从站句柄
 *  @param unit_id 从站地址（1-247，0=广播）
 *  @return EGW_OK 或错误码
 */
egw_err_t egw_modbus_server_add_unit(egw_modbus_server_t *server,
                                      uint8_t unit_id);

/** @brief 销毁从站实例 */
void egw_modbus_server_destroy(egw_modbus_server_t *server);

/** @brief 喂入接收字节（memcpy 路径）
 *
 * 字节写入内部环形缓冲区，尝试帧定界 + CRC 校验。
 * 帧就绪后自动解包 → 调 read/write 回调 → 组响应。 */
void egw_modbus_server_feed(egw_modbus_server_t *server,
                              const uint8_t *data, size_t len);

/** @brief 获取可写接收缓冲区（零拷贝路径）
 *
 * 返回内部环形缓冲区的空闲指针，
 * transport 直接读入后调用 commit 提交。
 * 响应待发送（sending=true）时返回 NULL。
 *
 *  @param s     从站句柄
 *  @param avail 输出可写字节数
 *  @return 可写指针，失败返回 NULL
 */
uint8_t *egw_modbus_server_reserve(egw_modbus_server_t *server, size_t *avail);

/** @brief 提交已写入的接收字节
 *
 * 触发帧定界 + 处理 + 生成响应。
 * 帧就绪后 sending = true，禁止处理下一帧。 */
void egw_modbus_server_commit(egw_modbus_server_t *server, size_t nbytes);

/** @brief 获取待发送的响应帧
 *
 * @param s   从站句柄
 * @param len 输出帧长度
 * @return 帧数据指针（response_sent 前有效）。不改变内部状态。
 */
const uint8_t *egw_modbus_server_get_response(egw_modbus_server_t *server,
                                               size_t *len);

/** @brief 标记响应已发送
 *
 * 清空响应缓冲区 + 解除 sending 锁定。
 * 之后立即尝试解析下一帧（accumulate during sending 的字节不会丢）。 */
void egw_modbus_server_response_sent(egw_modbus_server_t *server);

#endif /* EGW_MODBUS_SERVER_H */
