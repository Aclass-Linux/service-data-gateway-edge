#ifndef EGW_DEFS_H
#define EGW_DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef int32_t egw_err_t;

/* ── 核心类型枚举（从 X-macro 表展开）────────────── */

typedef uint8_t egw_ctype_t;

enum egw_ctype {
#define EGW_CORE_TYPE(name, val, desc) EGW_CTYPE_##name = (val),
    #include "egw_ctype.inc"
};

/* ── 错误码枚举（从 X-macro 表展开）─────────────── */
enum egw_err {
#define EGW_ERROR_CODE(name, val, desc) EGW_##name = (val),
    #include "egw_err.inc"
};

/*── 返回错误码宏───────────────────────────────────*/
#define EGW_RET_CODE(name) EGW_##name



/* ── 错误码 → 字符串（"EGW_ERR_OPEN (-10): open failed"）── */

const char *egw_err_str(egw_err_t err);

/* ── 总线值表示（无判别式 union，8 字节）────────────── */

typedef union {
    uint8_t  b;
    int16_t  i16;
    uint16_t u16;
    int32_t  i32;
    uint32_t u32;
    int64_t  i64;
    uint64_t u64;
    float    f32;
    double   f64;
    uint64_t raw;
} egw_value_t;

_Static_assert(sizeof(egw_value_t) == 8, "egw_value_t must be 8 bytes");

/* ── 字段映射（DB 列 → C struct 偏移/类型） ── */

typedef struct {
    const char *name;
    egw_ctype_t ctype;
    size_t      offset;
} egw_field_t;

/** @brief 快捷声明 egw_field_t 条目
 *  @param st       结构体类型
 *  @param col_name SQL 列名字符串（如 "device_id"）
 *  @param member   C 结构体成员名（裸标识符，给 offsetof 用）
 *  @param ctype_   egw_ctype_t 枚举值
 */
#define EGW_FIELD(st, col_name, member, ctype_) \
    { (col_name), (ctype_), offsetof(st, member) }

/* ── 一般数据类型 ──────────────────────────────── */

typedef struct {
    void   *data;
    size_t  len;
} egw_buf_t;

/* ── 编译器构造函数属性 ──────────────────────────── */

#if defined(__GNUC__) || defined(__clang__)
    #define EGW_CONSTRUCTOR(prio)   __attribute__((constructor(prio + 101)))
#else
    #define EGW_CONSTRUCTOR(prio)
#endif

#define EGW_EXPORT(func, prio) \
    static EGW_CONSTRUCTOR(prio) void func(void)

/* ── 日志宏 ────────────────────────────────────────── */

#define EGW_LOG_ERROR  0
#define EGW_LOG_WARN   1
#define EGW_LOG_INFO   2
#define EGW_LOG_DEBUG  3

#ifndef EGW_LOG_LEVEL
    #ifdef NDEBUG
        #define EGW_LOG_LEVEL  EGW_LOG_WARN
    #else
        #define EGW_LOG_LEVEL  EGW_LOG_DEBUG
    #endif
#endif

#define EGW_LOGE(fmt, ...) \
    do { \
        if (EGW_LOG_ERROR <= EGW_LOG_LEVEL) { \
            fprintf(stderr, "[E] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while (0)

#define EGW_LOGW(fmt, ...) \
    do { \
        if (EGW_LOG_WARN <= EGW_LOG_LEVEL) { \
            fprintf(stderr, "[W] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while (0)

#define EGW_LOGI(fmt, ...) \
    do { \
        if (EGW_LOG_INFO <= EGW_LOG_LEVEL) { \
            printf("[I] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while (0)

#define EGW_LOGD(fmt, ...) \
    do { \
        if (EGW_LOG_DEBUG <= EGW_LOG_LEVEL) { \
            printf("[D] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while (0)

#endif /* EGW_DEFS_H */
