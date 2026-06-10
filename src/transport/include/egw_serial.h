/**
 * @file egw_serial.h
 * @brief Transport 串口变种 —— 异步 UART 收发
 *
 * 基于 libuv uv_tty 实现串口的异步打开/关闭/读写。
 * 串口参数通过 egw_serial_params_t 传入。
 */

#ifndef EGW_SERIAL_H
#define EGW_SERIAL_H

#include "egw_transport.h"

#ifdef USE_JSON_CONFIG
typedef struct egw_conf egw_conf_t;
#endif

/* ── 串口参数 ──────────────────────────────────── */

typedef struct egw_serial_params {
    const char *path;
    int32_t     baud;
    char        parity;       /**< 'N', 'E', 'O' */
    int32_t     data_bits;    /**< 5, 6, 7, 8 */
    int32_t     stop_bits;    /**< 1, 2 */
} egw_serial_params_t;

/* ── 创建接口 ────────────────────────────────── */

/**
 * @brief 从参数创建并异步打开串口 Transport
 *
 * @param[out] tp      成功时写入 Transport 句柄
 * @param[in]  params  串口参数
 * @param[in]  cbs     回调集合
 * @return EGW_OK           已发起异步打开
 * @return EGW_ERR_INVAL  tp、params 或 cbs 为 NULL
 */
egw_err_t egw_serial_open(egw_transport_t **tp,
                           const egw_serial_params_t *params,
                           const egw_transport_cbs_t *cbs);

#ifdef USE_JSON_CONFIG

/**
 * @brief 从配置创建并异步打开串口 Transport
 *
 * 读取 JSON Pointer path 下的 serial port 配置，
 * 内部调用 egw_serial_open。
 *
 * @param[out] tp    成功时写入 Transport 句柄
 * @param[in]  cfg   配置句柄
 * @param[in]  path  JSON Pointer 路径，如 "/modbus/serial_ports/0"
 * @param[in]  cbs   回调集合
 * @return EGW_OK           已发起异步打开
 * @return EGW_ERR_INVAL  参数为 NULL
 */
egw_err_t egw_serial_from_config(egw_transport_t **tp,
                                  egw_conf_t *cfg, const char *path,
                                  const egw_transport_cbs_t *cbs);

#endif

#endif /* EGW_SERIAL_H */