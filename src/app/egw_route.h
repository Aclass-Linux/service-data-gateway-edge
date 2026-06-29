/**
 * @file egw_route.h
 * @brief 路由表（南向 ↔ 北向信号映射，协议无关）
 */

#ifndef EGW_ROUTE_H
#define EGW_ROUTE_H

#include "egw_ptable.h"

typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
    uint8_t  ctype;
} egw_route_entry_t;

/** @brief 路由表查找键（二分查找用） */
typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
} egw_route_key_t;

egw_err_t egw_route_load(egw_ptable_t *pt);

#endif /* EGW_ROUTE_H */
