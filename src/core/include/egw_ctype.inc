/* egw_ctype.inc — X-macro 核心类型表
 *
 * 用法：
 *   #define EGW_CORE_TYPE(name, val, desc) ...
 *   #include "egw_ctype.inc"
 *   #undef EGW_CORE_TYPE
 *
 * name  = 类型宏名（如 BOOL, I16）。宏定义负责拼上 EGW_CTYPE_ 前缀。
 * val   = 整数值（预留编号区间，便于后续插入）
 * desc  = 人类可读描述
 */

#ifndef EGW_CORE_TYPE
#error egw_ctype.inc requires EGW_CORE_TYPE macro to be defined before inclusion
#endif

EGW_CORE_TYPE(BOOL, 0x01, "boolean / coil")
EGW_CORE_TYPE(I16,  0x02, "int16")
EGW_CORE_TYPE(U16,  0x03, "uint16")
EGW_CORE_TYPE(I32,  0x04, "int32")
EGW_CORE_TYPE(U32,  0x05, "uint32")
EGW_CORE_TYPE(I64,  0x06, "int64")
EGW_CORE_TYPE(U64,  0x07, "uint64")
EGW_CORE_TYPE(F32,  0x08, "float32")
EGW_CORE_TYPE(F64,  0x09, "float64")
EGW_CORE_TYPE(STR,  0x0A, "string")

#undef EGW_CORE_TYPE
