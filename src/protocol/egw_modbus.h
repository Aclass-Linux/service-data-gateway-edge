#ifndef EGW_MODBUS_H
#define EGW_MODBUS_H

#include "egw_defs.h"
#include "egw_crc.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Modbus 协议常量 ────────────────────────────────── */

#define EGW_MODBUS_MAX_PDU      253
#define EGW_MODBUS_MAX_FRAME    260

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

/* ── 点表配置结构体 ──────────────────────────────────── */

#define EGW_MODBUS_MASTER_ENABLED           (1u << 0)
#define EGW_MODBUS_MASTER_HAS_SCALE_OFFSET  (1u << 1)
#define EGW_MODBUS_MASTER_HAS_DEADBAND      (1u << 2)

/** @brief 南向采集点（从站 → 本机）点表行 */
typedef struct {
    uint16_t device_id;          /**< 从站地址 */
    uint32_t sig_id;             /**< 信号 ID */
    uint8_t  func_code;          /**< Modbus 功能码 */
    uint16_t reg_addr;           /**< 寄存器起始地址 */
    uint16_t reg_count;          /**< 寄存器数量 */
    uint8_t  ctype;              /**< egw_ctype_t */
    uint32_t poll_interval_ms;   /**< 轮询间隔 */
    uint8_t  flags;              /**< EGW_MODBUS_MASTER_* 位 */
    float    scale;              /**< 缩放系数 */
    float    offset;             /**< 偏移 */
    float    deadband;           /**< 死区 */
} egw_modbus_master_t;

#define EGW_MODBUS_SLAVE_ENABLED           (1u << 0)
#define EGW_MODBUS_SLAVE_HAS_SCALE_OFFSET  (1u << 1)
#define EGW_MODBUS_SLAVE_HAS_DEADBAND      (1u << 2)

/** @brief 北向服务点（本机 → 主站）点表行 */
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

/** @brief 路由表行 */
typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
    uint8_t  ctype;
} egw_route_entry_t;

/* ── 传输层类型 ─────────────────────────────────────── */

typedef enum {
    EGW_MODBUS_RTU = 1,
    EGW_MODBUS_TCP = 2,
} egw_modbus_transport_t;

/* ── 传输层帧封装／解封装 ────────────────────────────── */

/** @brief 将 PDU 包装为传输层帧
 *  @param buf       输出缓冲区（至少 EGW_MODBUS_MAX_FRAME 字节）
 *  @param transport RTU 或 TCP
 *  @param unit_id   从站地址
 *  @param pdu       PDU 数据
 *  @param pdu_len   PDU 长度
 *  @param tid       TCP 事务 ID（RTU 忽略）
 *  @return 帧总长度，0 表示失败
 */
size_t egw_modbus_wrap_pdu(uint8_t *buf, egw_modbus_transport_t transport,
                            uint8_t unit_id,
                            const uint8_t *pdu, size_t pdu_len,
                            uint16_t tid);

/** @brief 从接收帧中解封装出 PDU + unit_id
 *  @param frame       原始帧
 *  @param len         帧长度
 *  @param transport   RTU 或 TCP
 *  @param unit_id_out 从站地址
 *  @param pdu_out     输出 PDU 缓冲区（至少 EGW_MODBUS_MAX_PDU 字节）
 *  @param pdu_len_out PDU 长度
 *  @return EGW_OK 或错误码
 */
egw_err_t egw_modbus_unwrap_frame(const uint8_t *frame, size_t len,
                                   egw_modbus_transport_t transport,
                                   uint8_t *unit_id_out,
                                   uint8_t *pdu_out, size_t *pdu_len_out);

/* ── PDU 构建（传输无关） ────────────────────────────── */

/** @brief 构建读请求 PDU（FC01-04）
 *  @param pdu    输出缓冲区（至少 5 字节）
 *  @param fc     功能码（1-4）
 *  @param addr   起始地址
 *  @param count  数量
 *  @return PDU 长度，0 表示失败
 */
size_t egw_modbus_build_read_pdu(uint8_t *pdu, uint8_t fc,
                                  uint16_t addr, uint16_t count);

/** @brief 构建写单线圈 PDU（FC05）
 *  @param pdu    输出缓冲区（至少 5 字节）
 *  @param addr   线圈地址
 *  @param value  0xFF00=ON, 0x0000=OFF
 *  @return PDU 长度
 */
size_t egw_modbus_build_write_single_coil_pdu(uint8_t *pdu,
                                               uint16_t addr, uint16_t value);

/** @brief 构建写单寄存器 PDU（FC06）
 *  @param pdu    输出缓冲区（至少 5 字节）
 *  @param addr   寄存器地址
 *  @param value  值
 *  @return PDU 长度
 */
size_t egw_modbus_build_write_single_reg_pdu(uint8_t *pdu,
                                              uint16_t addr, uint16_t value);

/** @brief 构建写多线圈 PDU（FC15）
 *  @param pdu     输出缓冲区（至少 EGW_MODBUS_MAX_PDU 字节）
 *  @param addr    起始地址
 *  @param values  线圈值（每字节 8 个线圈）
 *  @param count   线圈数量
 *  @return PDU 长度，0 表示失败
 */
size_t egw_modbus_build_write_multiple_coils_pdu(uint8_t *pdu, uint16_t addr,
                                                  const uint8_t *values,
                                                  uint16_t count);

/** @brief 构建写多寄存器 PDU（FC16）
 *  @param pdu     输出缓冲区（至少 EGW_MODBUS_MAX_PDU 字节）
 *  @param addr    起始地址
 *  @param values  寄存器值数组（CPU 字节序）
 *  @param count   寄存器数量（最大 123）
 *  @return PDU 长度，0 表示失败
 */
size_t egw_modbus_build_write_multiple_regs_pdu(uint8_t *pdu, uint16_t addr,
                                                 const uint16_t *values,
                                                 uint16_t count);

/** @brief 构建异常响应 PDU
 *  @param pdu  输出缓冲区
 *  @param fc   请求的功能码
 *  @param exc  异常码（EGW_MODBUS_EXC_*）
 *  @return PDU 长度
 */
size_t egw_modbus_build_exception_pdu(uint8_t *pdu, uint8_t fc, uint8_t exc);

/** @brief 构建读响应 PDU（FC01-04）
 *  @param pdu      输出缓冲区
 *  @param fc       功能码
 *  @param data     数据（线圈为大端位图，寄存器为大端字节序）
 *  @param data_len 数据长度
 *  @return PDU 长度，0 表示失败
 */
size_t egw_modbus_build_read_resp_pdu(uint8_t *pdu, uint8_t fc,
                                       const uint8_t *data, size_t data_len);

/* ── PDU 解析（传输无关） ────────────────────────────── */

/** @brief 解析读响应 PDU，提取寄存器值
 *  @param pdu      PDU 数据
 *  @param len      PDU 长度
 *  @param fc       期望的功能码
 *  @param regs     输出寄存器数组（CPU 字节序）
 *  @param max_regs 数组容量
 *  @return 实际寄存器数量（>0），负值为 Modbus 异常码的相反数，-1 为其他错误
 */
int egw_modbus_parse_read_pdu(const uint8_t *pdu, size_t len,
                               uint8_t fc,
                               uint16_t *regs, uint16_t max_regs);

/** @brief 解析写响应 PDU，验证回显
 *  @param pdu    PDU 数据
 *  @param len    PDU 长度
 *  @param fc     期望的功能码
 *  @return EGW_OK 或错误码
 */
egw_err_t egw_modbus_parse_write_pdu(const uint8_t *pdu, size_t len,
                                      uint8_t fc);

/** @brief 解析请求 PDU（服务器用）
 *  @param pdu          PDU 数据
 *  @param len          PDU 长度
 *  @param fc_out       功能码
 *  @param addr_out     地址
 *  @param count_out    数量
 *  @param data         写请求的数据区指针（FC05/06/0F/10）
 *  @param data_len_out 数据长度
 *  @return EGW_OK 或错误码
 */
egw_err_t egw_modbus_parse_request(const uint8_t *pdu, size_t len,
                                    uint8_t *fc_out,
                                    uint16_t *addr_out, uint16_t *count_out,
                                    const uint8_t **data,
                                    size_t *data_len_out);

/* ── 类型转换 ────────────────────────────────────────── */

/** @brief 寄存器数组 → egw_value_t（按 ctype 转换字节序）
 *  @param regs  寄存器数组（CPU 字节序）
 *  @param count 寄存器数量
 *  @param ctype 目标类型
 *  @param val   输出值
 *  @return EGW_OK 或错误码
 */
egw_err_t egw_modbus_regs_to_value(const uint16_t *regs, uint16_t count,
                                    egw_ctype_t ctype, egw_value_t *val);

/* ── 状态机枚举 ──────────────────────────────────────── */

typedef enum {
    EGW_MODBUS_IDLE,     /**< 空闲 */
    EGW_MODBUS_SENDING,  /**< 请求已就绪，待发送 */
    EGW_MODBUS_WAITING,  /**< 已发送，等待响应 */
    EGW_MODBUS_DONE,     /**< 响应解析完成 */
    EGW_MODBUS_ERROR,    /**< 超时／帧错误／异常响应 */
} egw_modbus_state_t;

/* ── Client（主站） ──────────────────────────────────── */

struct egw_proto_ctx;

/** @brief 请求完成回调
 *  @param unit_id 从站地址
 *  @param sig_id  信号 ID
 *  @param value   转换后的值
 *  @param arg     用户参数
 */
typedef void (*egw_modbus_done_cb)(uint8_t unit_id, uint32_t sig_id,
                                    egw_value_t value, void *arg);

/** @brief 单次 Modbus 请求上下文 */
typedef struct {
    uint8_t                 buf[EGW_MODBUS_MAX_FRAME];
    size_t                  len;
    struct egw_proto_ctx   *parser;
    egw_modbus_transport_t  transport;
    egw_modbus_state_t      state;
    int64_t                 deadline_ms;
    uint8_t                 unit_id;
    uint8_t                 fc;
    egw_ctype_t             ctype;
    uint32_t                read_timeout_ms;
    uint16_t                regs[128];
    int                     regs_parsed;
    uint32_t                sig_id;
    egw_value_t             value;
    egw_modbus_done_cb      done_cb;
    void                   *done_arg;
} egw_modbus_req_t;

/** @brief 初始化请求上下文（建帧，不执行 I/O）
 *  @param req             请求上下文
 *  @param transport       RTU 或 TCP
 *  @param unit_id         从站地址
 *  @param fc              功能码
 *  @param addr            寄存器地址
 *  @param count           寄存器数量
 *  @param ctype           目标数据类型
 *  @param read_timeout_ms 超时毫秒（0=不超时）
 *  @param sig_id          信号 ID
 *  @param cb              完成回调
 *  @param arg             回调用户参数
 */
void egw_modbus_req_init(egw_modbus_req_t *req,
                          egw_modbus_transport_t transport,
                          uint8_t unit_id, uint8_t fc,
                          uint16_t addr, uint16_t count,
                          egw_ctype_t ctype,
                          uint32_t read_timeout_ms,
                          uint32_t sig_id,
                          egw_modbus_done_cb cb, void *arg);

/** @brief 销毁请求上下文（释放 parser）
 *  @param req 请求上下文，可为 NULL
 */
void egw_modbus_req_destroy(egw_modbus_req_t *req);

/** @brief 标记请求已发送，进入等待状态
 *  不执行 I/O。调用方在发送帧数据后调用此函数，并传入当前时间。
 *  @param req    请求上下文
 *  @param now_ms 当前时间（毫秒）
 */
void egw_modbus_req_send(egw_modbus_req_t *req, uint32_t now_ms);

/** @brief 喂入接收到的原始字节
 *  @param req  请求上下文
 *  @param data 原始字节
 *  @param len  字节数
 */
void egw_modbus_req_feed(egw_modbus_req_t *req,
                          const uint8_t *data, size_t len);

/** @brief 驱动状态机（超时检测 + 回调触发）
 *  @param req    请求上下文
 *  @param now_ms 当前时间（毫秒）
 *  @return 当前状态
 */
egw_modbus_state_t egw_modbus_req_process(egw_modbus_req_t *req,
                                           uint32_t now_ms);

/* ── Server（从站） ──────────────────────────────────── */

/** @brief 读寄存器回调
 *  @param address  寄存器地址
 *  @param quantity 数量
 *  @param regs_out 输出缓冲区（CPU 字节序）
 *  @param unit_id  从站地址
 *  @param arg      用户参数
 *  @return EGW_OK 或错误码
 */
typedef egw_err_t (*egw_modbus_srv_read_cb)(uint16_t address, uint16_t quantity,
                                              uint16_t *regs_out,
                                              uint8_t unit_id, void *arg);

/** @brief 写寄存器回调
 *  @param address  寄存器地址
 *  @param quantity 数量
 *  @param regs     寄存器值（CPU 字节序）
 *  @param unit_id  从站地址
 *  @param arg      用户参数
 *  @return EGW_OK 或错误码
 */
typedef egw_err_t (*egw_modbus_srv_write_cb)(uint16_t address, uint16_t quantity,
                                               const uint16_t *regs,
                                               uint8_t unit_id, void *arg);

typedef struct egw_modbus_server egw_modbus_server_t;

/** @brief 创建从站实例
 *  @param transport RTU 或 TCP
 *  @param unit_id   本机从站地址
 *  @param read_cb   读回调
 *  @param write_cb  写回调（可为 NULL）
 *  @param arg       回调用户参数
 *  @return 句柄，失败返回 NULL
 */
egw_modbus_server_t *egw_modbus_server_create(egw_modbus_transport_t transport,
                                                uint8_t unit_id,
                                                egw_modbus_srv_read_cb read_cb,
                                                egw_modbus_srv_write_cb write_cb,
                                                void *arg);

/** @brief 销毁从站实例
 *  @param s 从站句柄，可为 NULL
 */
void egw_modbus_server_destroy(egw_modbus_server_t *s);

/** @brief 喂入接收到的原始字节
 *  @param s    从站句柄
 *  @param data 原始字节
 *  @param len  字节数
 */
void egw_modbus_server_feed(egw_modbus_server_t *s,
                             const uint8_t *data, size_t len);

/** @brief 检查是否有响应待发送
 *  @param s 从站句柄
 *  @return true 表示有响应就绪
 */
bool egw_modbus_server_response_ready(const egw_modbus_server_t *s);

/** @brief 获取待发送的响应帧
 *  @param s   从站句柄
 *  @param len 输出帧长度
 *  @return 帧数据指针（调用 response_sent 前有效）
 */
const uint8_t *egw_modbus_server_get_response(egw_modbus_server_t *s,
                                               size_t *len);

/** @brief 标记响应已发送，清空内部响应缓冲区
 *  @param s 从站句柄
 */
void egw_modbus_server_response_sent(egw_modbus_server_t *s);

#endif /* EGW_MODBUS_H */
