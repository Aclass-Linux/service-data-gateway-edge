/**
 * @file egw_transport.h
 * @brief Transport 基类 —— 异步字节流 I/O 通道
 *
 * 所有 Transport 变种（串口、TCP、UDP 等）共享此基类。
 * 基于 libuv event loop，全部操作异步回调。
 * 多态通过 vtable（egw_transport_ops）实现。
 *
 * 生命周期：
 *   1. 调用具体变种的 open（如 egw_serial_open），传入回调和参数
 *   2. open 异步完成后触发 on_open 回调
 *   3. 数据到达触发 on_data 回调
 *   4. 写入完成触发 on_write 回调
 *   5. 关闭完成后触发 on_close 回调
 *
 * 回调指针可为 NULL（表示不关心该事件），
 * 但 on_data 为 NULL 会导致接收到的数据被丢弃。
 */

#ifndef EGW_TRANSPORT_H
#define EGW_TRANSPORT_H

#include "egw_defs.h"
#include <stddef.h>
#include <stdint.h>

/* ── 前向声明 ──────────────────────────────────── */

typedef struct egw_transport egw_transport_t;

/* ── 回调类型 ──────────────────────────────────── */

typedef void (*egw_transport_open_cb)(egw_transport_t *tp, egw_err_t err);
typedef void (*egw_transport_data_cb)(egw_transport_t *tp, const void *buf, size_t len);
typedef void (*egw_transport_write_cb)(egw_transport_t *tp, egw_err_t err);
typedef void (*egw_transport_close_cb)(egw_transport_t *tp, egw_err_t err);

/**
 * @brief Transport 事件回调集合
 *
 * 创建 Transport 时一并注册，运行时由 Transport 内部在事件发生时调用。
 * user_data 由调用方设置，Transport 不做解释。
 */
typedef struct egw_transport_cbs {
    egw_transport_open_cb   on_open;
    egw_transport_data_cb   on_data;
    egw_transport_write_cb  on_write;
    egw_transport_close_cb  on_close;
    void                   *user_data;
} egw_transport_cbs_t;

/* ── vtable ────────────────────────────────────── */

struct egw_transport_ops {
    egw_err_t (*open)(egw_transport_t *tp);
    egw_err_t (*close)(egw_transport_t *tp);
    egw_err_t (*write)(egw_transport_t *tp, const void *buf, size_t len);
};

/* ── 基类 ────────────────────────────────────── */

struct egw_transport {
    const struct egw_transport_ops *ops;
    uint32_t id;
    uint32_t seq;
    egw_transport_cbs_t cbs;
};

/* ── 通用接口 ────────────────────────────────── */

/**
 * @brief 异步关闭 Transport
 *
 * 关闭完成后触发 cbs.on_close 回调。
 * 传入 NULL 为无操作。
 *
 * @return EGW_OK           已发起关闭
 * @return EGW_ERR_HANDLER  tp 为 NULL
 */
egw_err_t egw_transport_close(egw_transport_t *tp);

/**
 * @brief 异步写入数据
 *
 * 写入完成后触发 cbs.on_write 回调。
 * buf 必须在回调触发前保持有效。
 *
 * @param[in] tp   Transport 句柄
 * @param[in] buf  待写入数据
 * @param[in] len  数据长度
 * @return EGW_OK           已发起写入
 * @return EGW_ERR_HANDLER  tp 或 buf 为 NULL，len 为 0
 * @return EGW_ERR_TP_BUSY  上一次写入尚未完成
 */
egw_err_t egw_transport_write(egw_transport_t *tp, const void *buf, size_t len);

#endif /* EGW_TRANSPORT_H */