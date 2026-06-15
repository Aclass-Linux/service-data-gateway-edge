#ifndef EGW_DEFS_H
#define EGW_DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef int32_t egw_err_t;

/* ── 错误码枚举（从 X-macro 表展开）─────────────── */
enum egw_err {
#define EGW_ERROR_CODE(name, val, desc) EGW_##name = (val),
    #include "egw_err.inc"
};

/*── 返回错误码宏───────────────────────────────────*/
#define EGW_RETURN_CODE(name) EGW_##name



/* ── 错误码 → 字符串（"EGW_ERR_OPEN (-10): open failed"）── */

const char *egw_err_str(egw_err_t err);

/* ── 编译器构造函数属性 ──────────────────────────── */

#if defined(__GNUC__) || defined(__clang__)
    #define EGW_CONSTRUCTOR(prio)   __attribute__((constructor(prio + 101)))
#else
    #define EGW_CONSTRUCTOR(prio)
#endif

#define EGW_EXPORT(func, prio) \
    static EGW_CONSTRUCTOR(prio) void func(void)

#endif /* EGW_DEFS_H */
