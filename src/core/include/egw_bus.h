/**
 * @file egw_bus.h
 * @brief 线程内发布订阅总线
 *
 * 订阅粒度 (device_id, sig_id) 复合键。
 * 发布时同步调用所有匹配订阅者的回调，回调必须非阻塞。
 * 第一版单线程，不引入锁。
 */

#ifndef EGW_BUS_H
#define EGW_BUS_H

#include "egw_defs.h"

typedef struct egw_bus egw_bus_t;

typedef void (*egw_bus_cb)(uint16_t device_id, uint32_t sig_id,
                            egw_value_t value, void *data);

egw_bus_t *egw_bus_create(void);
void       egw_bus_destroy(egw_bus_t *bus);

egw_err_t  egw_bus_subscribe(egw_bus_t *bus, uint16_t device_id,
                              uint32_t sig_id, egw_bus_cb cb, void *data);
egw_err_t  egw_bus_unsubscribe(egw_bus_t *bus, uint16_t device_id,
                                uint32_t sig_id, egw_bus_cb cb);

void       egw_bus_publish(egw_bus_t *bus, uint16_t device_id,
                            uint32_t sig_id, egw_value_t value);

#endif
