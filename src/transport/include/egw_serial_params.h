/**
 * @file egw_serial_params.h
 * @brief 串口参数结构体
 */

#ifndef EGW_SERIAL_PARAMS_H
#define EGW_SERIAL_PARAMS_H

#include <stdint.h>

typedef struct egw_serial_params {
    const char *path;
    int32_t     baud;
    char        parity;       /**< 'N', 'E', 'O' */
    int32_t     data_bits;    /**< 5, 6, 7, 8 */
    int32_t     stop_bits;    /**< 1, 2 */
} egw_serial_params_t;

#endif /* EGW_SERIAL_PARAMS_H */
