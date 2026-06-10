/**
 * @file egw_defs.h
 * @brief DataGatewayHub 全局公共定义
 *
 * 全项目共享的错误码、编译器包装宏。
 * 不依赖任何项目内部头文件，可被所有模块包含。
 */

#ifndef EGW_DEFS_H
#define EGW_DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 全局错误码
 *
 * 所有模块共享同一套错误码。0 = 成功，负值 = 错误。
 * 顺序编号，新增错误码在末尾追加，保持现有值不变。
 * 模块归属通过宏命名前缀区分（如 EGW_ERR_MQTT_*）。
 */
typedef int32_t egw_err_t;

#define EGW_OK                  (0)

#define EGW_ERR_FILE_NOT_FOUND  (-1)
#define EGW_ERR_PARSE           (-2)
#define EGW_ERR_MISSING_KEY     (-3)
#define EGW_ERR_REGISTRY_FULL   (-4)
#define EGW_ERR_HANDLER         (-5)

/* ── Transport 错误码 ──────────────────────────── */
#define EGW_ERR_TP_OPEN         (-10)
#define EGW_ERR_TP_CLOSE        (-11)
#define EGW_ERR_TP_WRITE        (-12)
#define EGW_ERR_TP_READ         (-13)
#define EGW_ERR_TP_BUSY         (-14)
#define EGW_ERR_TP_TIMEOUT      (-15)


/**
 * @brief 编译器构造函数属性。
 *
 * 将修饰的函数放入 .init_array 段，在 main() 之前按优先级自动执行。
 *
 * - prio 越小越先执行（0–65535）
 * - 同一优先级的函数间执行顺序未定义
 * - EGW_CONSTRUCTOR(0) 对应实际 __attribute__((constructor(101)))，
 *   自动避开编译器保留区间（0-100）
 * - EGW_EXPORT 默认使用 EGW_CONSTRUCTOR(0)，模块间无依赖冲突
 *
 * @param[in] prio 初始化优先级
 *
 * CMSIS 风格：换编译器时只需修改此映射，EGW_EXPORT 无需改动。
 */
/* GCC extension, not C11 */
#if defined(__GNUC__) || defined(__clang__)
    #define EGW_CONSTRUCTOR(prio)   __attribute__((constructor(prio + 101)))
#else
    /* 非 gcc/clang 编译器需自行实现 */
    #define EGW_CONSTRUCTOR(prio)
#endif

/**
 * @brief 模块自动初始化宏。
 *
 * 基于 EGW_CONSTRUCTOR，在 main() 之前自动执行指定函数。
 *
 * 用法：
 * @code
 * EGW_EXPORT(egw_init_mqtt, 0) {
 *     // 初始化代码
 * }
 * @endcode
 *
 * @param func  函数名
 * @param prio  初始化优先级（0 = 默认）
 */
#define EGW_EXPORT(func, prio) \
    static EGW_CONSTRUCTOR(prio) void func(void)

#endif /* EGW_DEFS_H */
