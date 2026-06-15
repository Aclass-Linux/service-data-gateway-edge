#ifndef EGW_LOOP_H
#define EGW_LOOP_H

/**
 * @file egw_loop.h
 * @brief Event loop abstraction over libuv
 *
 * Core layer封装libuv事件循环，成为事件循环的唯一所有者。
 * 其他层（Transport、Protocol、App）通过此API访问事件循环，
 * 不直接调用libuv。
 *
 * 参考：ADR-0006 Core层持有事件循环
 */

#include "egw_defs.h"

/* ── 不透明句柄 ────────────────────────────────────── */

typedef struct egw_loop egw_loop_t;

/* ── 生命周期 ──────────────────────────────────────── */

/**
 * @brief 创建事件循环
 * @return 成功返回循环句柄，失败返回NULL
 */
egw_loop_t *egw_loop_create(void);

/**
 * @brief 运行事件循环（阻塞）
 * @param loop 循环句柄
 * @return EGW_OK 正常退出，其他值表示错误
 */
egw_err_t egw_loop_run(egw_loop_t *loop);

/**
 * @brief 停止事件循环
 * @param loop 循环句柄
 *
 * 此函数可以从信号处理器或其他回调中调用，
 * 会在当前循环迭代结束后退出egw_loop_run()。
 */
void egw_loop_stop(egw_loop_t *loop);

/**
 * @brief 销毁事件循环
 * @param loop 循环句柄
 *
 * 必须在egw_loop_run()返回后调用。
 * 会关闭所有未关闭的句柄并释放资源。
 */
void egw_loop_destroy(egw_loop_t *loop);

/* ── 内部访问接口（仅供Core层内部模块使用）────────── */

/**
 * @brief 获取底层uv_loop_t指针（仅供Core层内部使用）
 * @param loop 循环句柄
 * @return uv_loop_t指针
 *
 * 此函数仅供Core层内部模块（timer、signal等）使用，
 * 外部模块不应调用。
 */
void *egw_loop_get_uv_loop(egw_loop_t *loop);

#endif /* EGW_LOOP_H */
