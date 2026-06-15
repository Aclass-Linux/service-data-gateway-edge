/**
 * @file egw_runtime.h
 * @brief 线程内运行时单例
 *
 * 聚合 loop、bus、ptable，通过 egw_runtime_current() 访问。
 * 第一版单线程，runtime 在 main 线程创建并绑定。
 */

#ifndef EGW_RUNTIME_H
#define EGW_RUNTIME_H

#include "egw_defs.h"
#include "egw_loop.h"
#include "egw_bus.h"

typedef struct egw_runtime egw_runtime_t;

egw_runtime_t *egw_runtime_create(egw_loop_t *loop, egw_bus_t *bus);
void           egw_runtime_destroy(egw_runtime_t *rt);
egw_runtime_t *egw_runtime_current(void);

egw_loop_t    *egw_runtime_loop(egw_runtime_t *rt);
egw_bus_t     *egw_runtime_bus(egw_runtime_t *rt);

#endif
