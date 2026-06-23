#ifndef EGW_CRC_H
#define EGW_CRC_H

#include <stdint.h>
#include <stddef.h>

/** @brief Modbus CRC — 查表法（推荐，更快）
 *  @param data 数据缓冲区
 *  @param len  数据长度
 *  @return CRC 值
 */
uint16_t egw_crc_modbus_table(const uint8_t *data, size_t len);

/** @brief Modbus CRC — 计算法（代码量小，适合代码空间受限场景）
 *  @param data 数据缓冲区
 *  @param len  数据长度
 *  @return CRC 值
 */
uint16_t egw_crc_modbus_calc(const uint8_t *data, size_t len);

#endif /* EGW_CRC_H */
