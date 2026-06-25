/**
 * @file egw_protocol.h
 * @brief 协议帧定界（通用 handle + 协议特定 open）
 *
 * 设计与 transport 层对齐：
 * - 不透明 handle（`egw_proto_handle_t`），fopen 风格 open/close
 * - 协议特定 open 函数（如 `egw_proto_modbus_open`）分配 handle + 设置 vtable
 * - 通用 API（reserve/commit/feed）通过 vtable 分派到协议特定实现
 *
 * 两种喂入方式：
 *
 * 1. **reserve/commit**（推荐，零拷贝）—— io_uring registered-buffer 模式：
 *    @code
 *    size_t avail = 0;
 *    uint8_t *wp = egw_proto_reserve(h, &avail);
 *    ssize_t n = read(fd, wp, avail);
 *    egw_proto_result_t r = egw_proto_commit(h, (size_t)n);
 *    @endcode
 *
 * 2. **feed**（兼容）—— 数据已在调用方缓冲区时使用
 */

#ifndef EGW_PROTOCOL_H
#define EGW_PROTOCOL_H

#include "egw_defs.h"

/* ── 不透明句柄 ─────────────────────────────────────── */

typedef struct egw_proto_handle egw_proto_handle_t;

/* ── 解析方向 ───────────────────────────────────────── */

/**
 * @brief 帧解析方向
 *
 * 同一功能码的请求帧和响应帧长度结构不同，
 * protocol 层需要知道方向才能正确计算期望帧长度。
 */
typedef enum {
    EGW_PROTO_DIR_RESPONSE = 0,  /**< 主站（client）：解析从站响应 */
    EGW_PROTO_DIR_REQUEST  = 1,  /**< 从站（server）：解析主站请求 */
} egw_proto_dir_t;

/* ── 默认缓冲区大小 ─────────────────────────────────── */

#define EGW_PROTO_DEFAULT_BUF_SIZE  260

/* ── 推入结果 ───────────────────────────────────────── */

typedef enum {
    EGW_PROTO_NEED_MORE,
    EGW_PROTO_FRAME_READY,
    EGW_PROTO_FRAME_ERROR,
} egw_proto_result_t;

/* ── 通用 API（协议无关） ────────────────────────────── */

/**
 * @brief 预留可写缓冲区指针（零拷贝路径）
 * @param h     协议句柄
 * @param avail 输出可写字节数
 * @return 可写指针（指向句柄内部缓冲区末尾），失败返回 NULL
 */
uint8_t *egw_proto_reserve(egw_proto_handle_t *h, size_t *avail);

/**
 * @brief 提交 reserve 后实际写入的字节数，触发帧定界 + 校验
 * @param h 协议句柄
 * @param n 实际写入字节数
 * @return EGW_PROTO_FRAME_READY / NEED_MORE / FRAME_ERROR
 */
egw_proto_result_t egw_proto_commit(egw_proto_handle_t *h, size_t n);

/**
 * @brief 推入原始字节（兼容路径，内部 memcpy）
 * @param h    协议句柄
 * @param data 原始字节缓冲区
 * @param len  字节数
 * @return EGW_PROTO_FRAME_READY / NEED_MORE / FRAME_ERROR
 */
egw_proto_result_t egw_proto_feed(egw_proto_handle_t *h,
                                    const uint8_t *data, size_t len);

/**
 * @brief 获取已就绪的完整帧指针
 * @param h         协议句柄
 * @param frame_len 输出帧长度
 * @return 帧数据指针（下次喂入前有效）
 */
const uint8_t *egw_proto_get_frame(egw_proto_handle_t *h, size_t *frame_len);

/**
 * @brief 重置句柄（丢弃当前累积的字节）
 * @param h 协议句柄
 */
void egw_proto_reset(egw_proto_handle_t *h);

/**
 * @brief 关闭并释放协议句柄
 * @param h 协议句柄，可为 NULL
 */
void egw_proto_close(egw_proto_handle_t *h);

/* ── Modbus 协议特定 open ───────────────────────────── */

typedef struct {
    size_t           buf_size;  /**< 缓冲区大小，0 = 默认 */
    egw_proto_dir_t  dir;       /**< 解析方向 */
} egw_proto_modbus_params_t;

/**
 * @brief 创建 Modbus 帧定界句柄
 * @param params 参数（buf_size + dir），不可为 NULL
 * @return 句柄，失败返回 NULL
 */
egw_proto_handle_t *egw_proto_modbus_open(const egw_proto_modbus_params_t *params);

#endif /* EGW_PROTOCOL_H */
