/* egw_err.inc — X-macro 错误码表
 *
 * 用法：
 *   #define EGW_ERROR_CODE(name, value, desc) ...
 *   #include "egw_err.inc"
 *   #undef EGW_ERROR_CODE
 *
 * name   = 宏名（如 OK, ERR_OPEN）。宏定义负责拼上 EGW_ 前缀。
 * value  = 整数值
 * desc   = 人类可读描述
 */

#ifndef EGW_ERROR_CODE
#error egw_err.inc requires EGW_ERROR_CODE macro to be defined before inclusion
#endif

EGW_ERROR_CODE(OK,           0,   "success")
EGW_ERROR_CODE(ERR_NOTFOUND, -1,  "resource not found")
EGW_ERROR_CODE(ERR_PARSE,    -2,  "parse failure")
EGW_ERROR_CODE(ERR_INVAL,    -3,  "invalid argument")
EGW_ERROR_CODE(ERR_NOMEM,    -4,  "out of memory")
EGW_ERROR_CODE(ERR_OPEN,     -10, "open failed")
EGW_ERROR_CODE(ERR_CLOSE,    -11, "close failed")
EGW_ERROR_CODE(ERR_WRITE,    -12, "write failed")
EGW_ERROR_CODE(ERR_READ,     -13, "read failed")
EGW_ERROR_CODE(ERR_BUSY,     -14, "resource busy")
EGW_ERROR_CODE(ERR_TIMEOUT,  -15, "operation timed out")

#undef EGW_ERROR_CODE
