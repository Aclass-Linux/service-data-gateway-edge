#ifndef EGW_MODBUS_TRANSPORT_H
#define EGW_MODBUS_TRANSPORT_H

#include "egw_modbus_core.h"
#include <stddef.h>
#include <stdint.h>

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

/* ── Server 输运适配器（抹掉 transport/tid 参数，注入用） ─ */

typedef size_t (*egw_modbus_ser_wrap_fn)(uint8_t *buf, uint8_t unit_id,
                                          const uint8_t *pdu, size_t pdu_len);

typedef egw_err_t (*egw_modbus_ser_unwrap_fn)(const uint8_t *frame, size_t len,
                                               uint8_t *unit_id_out,
                                               uint8_t *pdu_out,
                                               size_t *pdu_len_out);

size_t egw_modbus_ser_wrap_rtu(uint8_t *buf, uint8_t unit_id,
                                const uint8_t *pdu, size_t pdu_len);
size_t egw_modbus_ser_wrap_tcp(uint8_t *buf, uint8_t unit_id,
                                const uint8_t *pdu, size_t pdu_len);
egw_err_t egw_modbus_ser_unwrap_rtu(const uint8_t *frame, size_t len,
                                     uint8_t *unit_id_out,
                                     uint8_t *pdu_out, size_t *pdu_len_out);
egw_err_t egw_modbus_ser_unwrap_tcp(const uint8_t *frame, size_t len,
                                     uint8_t *unit_id_out,
                                     uint8_t *pdu_out, size_t *pdu_len_out);

#endif /* EGW_MODBUS_TRANSPORT_H */
