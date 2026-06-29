#ifndef EGW_MODBUS_CORE_H
#define EGW_MODBUS_CORE_H

#include "egw_defs.h"
#include <stdint.h>
#include <stddef.h>

/* ── 帧解析方向 ─────────────────────────────────────── */

/** @brief 帧定界方向
 *
 * 同一功能码的请求帧和响应帧长度结构不同，
 * 解析器需要知道方向才能正确计算期望帧长度。
 */
typedef enum {
    EGW_PROTO_DIR_RESPONSE = 0,  /**< 主站侧：解析从站发来的响应帧 */
    EGW_PROTO_DIR_REQUEST  = 1,  /**< 从站侧：解析主站发来的请求帧 */
} egw_proto_dir_t;

/* ── 帧定界结果 ─────────────────────────────────────── */

/** @brief 帧解析返回值 */
typedef enum {
    EGW_PROTO_NEED_MORE,    /**< 已收字节不足一个完整帧，继续收 */
    EGW_PROTO_FRAME_READY,  /**< 收到完整帧且校验通过 */
    EGW_PROTO_FRAME_ERROR,  /**< CRC 校验失败或帧格式错误，丢弃累积数据 */
} egw_proto_result_t;

/* ── Modbus 协议常量 ────────────────────────────────── */

#define EGW_MODBUS_MAX_PDU      253   /**< Modbus PDU 最大长度（RFC 标准） */
#define EGW_MODBUS_MAX_FRAME    260   /**< ADU 最大长度（PDU + 地址 + CRC/MBAP） */

/* ── 功能码 ─────────────────────────────────────────── */

#define EGW_MODBUS_FC_READ_COILS               0x01  /**< 读线圈 */
#define EGW_MODBUS_FC_READ_DISCRETE_INPUTS     0x02  /**< 读离散输入 */
#define EGW_MODBUS_FC_READ_HOLDING_REGISTERS   0x03  /**< 读保持寄存器 */
#define EGW_MODBUS_FC_READ_INPUT_REGISTERS     0x04  /**< 读输入寄存器 */
#define EGW_MODBUS_FC_WRITE_SINGLE_COIL        0x05  /**< 写单线圈 */
#define EGW_MODBUS_FC_WRITE_SINGLE_REGISTER    0x06  /**< 写单寄存器 */
#define EGW_MODBUS_FC_WRITE_MULTIPLE_COILS     0x0F  /**< 写多线圈 */
#define EGW_MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10  /**< 写多寄存器 */

/* ── 异常码 ─────────────────────────────────────────── */

#define EGW_MODBUS_EXC_ILLEGAL_FUNCTION      1   /**< 非法功能码 */
#define EGW_MODBUS_EXC_ILLEGAL_DATA_ADDR     2   /**< 非法数据地址 */
#define EGW_MODBUS_EXC_ILLEGAL_DATA_VAL      3   /**< 非法数据值 */
#define EGW_MODBUS_EXC_SLAVE_DEVICE_FAILURE  4   /**< 从站设备故障 */
#define EGW_MODBUS_EXC_ACKNOWLEDGE           5   /**< 确认（长操作） */
#define EGW_MODBUS_EXC_SLAVE_DEVICE_BUSY     6   /**< 从站忙 */
#define EGW_MODBUS_EXC_NEGATIVE_ACK          7   /**< 否定确认 */
#define EGW_MODBUS_EXC_MEMORY_PARITY_ERROR   8   /**< 存储器校验错 */
#define EGW_MODBUS_EXC_GATEWAY_PATH         10   /**< 网关路径不可用 */
#define EGW_MODBUS_EXC_GATEWAY_TARGET       11   /**< 网关目标无响应 */

/* ── 传输层类型 ─────────────────────────────────────── */

/** @brief Modbus 底层传输方式 */
typedef enum {
    EGW_MODBUS_RTU = 1,  /**< 串行 RTU（CRC-16 校验，无头） */
    EGW_MODBUS_TCP = 2,  /**< TCP/IP（MBAP 头部，无 CRC） */
} egw_modbus_transport_t;

/* ── 构建读响应 PDU（FC01-04，服务器用） ────────────────── */
size_t egw_modbus_build_read_resp_pdu(uint8_t *pdu, uint8_t fc,
                                       const uint8_t *data, size_t data_len);

/* ── PDU 解析（传输无关） ────────────────────────────── */

/** @brief 解析读响应 PDU，提取寄存器值
 *  @return >0                实际寄存器数量
 *          -1                参数错误/功能码不匹配/长度异常
 *          -(exc + 100)      Modbus 异常响应（-101 ~ -111）
 */
int egw_modbus_parse_read_pdu(const uint8_t *pdu, size_t len,
                               uint8_t fc,
                               uint16_t *regs, uint16_t max_regs);

/** @brief 解析写响应 PDU，验证回显 */
egw_err_t egw_modbus_parse_write_pdu(const uint8_t *pdu, size_t len,
                                      uint8_t fc);

/** @brief 解析请求 PDU（服务器用） */
egw_err_t egw_modbus_parse_request(const uint8_t *pdu, size_t len,
                                    uint8_t *fc_out,
                                    uint16_t *addr_out, uint16_t *count_out,
                                    const uint8_t **data,
                                    size_t *data_len_out);

/* ── 帧封装／解封装（输运层） ────────────────────────── */

/** @brief 编码模式 */
typedef enum {
    EGW_ENCODE_FROM_PARAMS,   /**< 从 funccode/addr/count/data 构建 PDU 再打包 */
    EGW_ENCODE_PDU,           /**< 已有 PDU，直接打包 */
    EGW_ENCODE_EXCEPTION,     /**< 从 funccode + exc_code 构建异常 PDU 再打包 */
} egw_modbus_encode_type_t;

/** @brief encode 统一入参
 *
 * 三个模式共用此结构，通过 type 字段区分。
 * - FROM_PARAMS: 填 unit_id/funccode/addr/count/data
 * - PDU:         填 unit_id/pdu/pdu_len
 * - EXCEPTION:   填 unit_id/funccode/exc_code
 *
 * buf/cap：非 NULL 时 encode 写入此缓冲区；NULL 时 encode 自行 malloc
 * tid：TCP Transaction Identifier，RTU 时忽略 */
typedef struct {
    egw_modbus_encode_type_t  type;
    uint16_t                  tid;       /* TCP Tid，RTU 忽略 */

    uint8_t        unit_id;
    uint8_t        funccode;
    uint16_t       addr;
    uint16_t       count;
    const void    *data;
    size_t         data_len;

    const uint8_t *pdu;       /* EGW_ENCODE_PDU 时使用 */
    size_t         pdu_len;

    uint8_t        exc_code;  /* EGW_ENCODE_EXCEPTION 时使用 */

    uint8_t       *buf;       /* 外部缓冲区，NULL 则内部分配 */
    size_t         cap;
} egw_modbus_encode_params_t;

typedef struct {
    uint8_t           unit_id;
    uint16_t          addr;
    const uint16_t   *regs;
    int               reg_count;
} egw_modbus_result_t;

typedef void (*egw_modbus_done_cb)(const egw_modbus_result_t *result, void *arg);

/* ── 输运层统一打包/解包 ──────────────────────────── */

/** @brief 编码完整 ADU
 *  @return 完整 ADU 指针（成功）或 NULL。
 *          params->buf 非 NULL 时返回该指针，否则返回 malloc 的指针。调用方 free。 */
uint8_t* egw_modbus_encode(egw_modbus_transport_t transport,
                            const egw_modbus_encode_params_t *params,
                            OUT size_t *out_len);

/** @brief 解码完整帧，提取 unit_id + PDU */
egw_err_t egw_modbus_decode(egw_modbus_transport_t transport,
                             const uint8_t *frame, size_t len,
                             OUT uint8_t *unit_id_out,
                             OUT uint8_t *pdu_out,
                             OUT size_t *pdu_len_out);

#endif /* EGW_MODBUS_CORE_H */
