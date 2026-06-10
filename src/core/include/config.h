/**
 * @file config.h
 * @brief DataGatewayHub 配置访问层
 *
 * 提供 JSON 配置文件的加载和 JSON Pointer（RFC 6901）实时查询。
 * 模块通过 handle 直接查询 cJSON 树，不缓存到 struct。
 *
 * 路径语法遵循 RFC 6901：
 *   - 以 "/" 开头，每层用 "/" 分隔
 *   - 数组下标直接用数字，如 "/modbus/serial_ports/0/path"
 *   - 不支持含 "~" 或 "/" 的键名（无需转义）
 *
 * 线程安全：egw_conf_t 句柄不可跨线程传递或共享。
 * 多线程环境下每个线程必须独立加载自己的句柄。
 *
 * 编译选项：
 *   - USE_JSON_CONFIG=ON（默认）：启用 cJSON 后端，配置从 JSON 文件加载
 *   - USE_JSON_CONFIG=OFF：禁用 cJSON 后端，所有查询返回 def 默认值
 *
 * TODO: 无 cJSON 后端时数组默认值问题待解决：
 *   单靠 def 参数无法表达固定拓扑（如"N 个设备"），
 *   后续通过 Python 脚本从 JSON 配置生成 .c/.h 文件，将默认配置编译进固件
 */

#ifndef EGW_CONFIG_H
#define EGW_CONFIG_H

#include "egw_defs.h"
#include <stdbool.h>

#ifdef USE_JSON_CONFIG
/**
 * @brief 配置句柄（不透明）
 *
 * 通过 egw_conf_load() 创建，egw_conf_free() 释放。
 * 调用者不应直接访问结构体字段。
 */
typedef struct egw_conf egw_conf_t;

/**
 * @brief 从 JSON 文件加载配置
 *
 * @param[in]  path  JSON 配置文件路径
 * @param[out] cfg   成功时写入配置句柄，调用者负责释放
 * @return EGW_OK             成功
 * @return EGW_ERR_NOTFOUND  文件不存在或无法打开
 * @return EGW_ERR_PARSE       JSON 解析失败或根元素不是对象
 * @return EGW_ERR_INVAL     参数为 NULL 或内存分配失败
 */
egw_err_t    egw_conf_load(const char *path, egw_conf_t **cfg);

/**
 * @brief 释放配置句柄
 *
 * 释放后 cfg 指向的内存不可再访问。传入 NULL 为无操作。
 *
 * @param[in] cfg  配置句柄，可为 NULL
 */
void         egw_conf_free(egw_conf_t *cfg);

/**
 * @brief 进入子树，后续查询从该节点开始
 *
 * 调用后所有 get/array 查询以该节点为基准（相对路径）。
 * 传空字符串 "" 可回到文档根。
 *
 * @param[in] cfg       配置句柄
 * @param[in] key_path  JSON Pointer 路径，"" 表示回到文档根
 * @return EGW_OK             成功
 * @return EGW_ERR_INVAL    参数为 NULL
 * @return EGW_ERR_NOTFOUND  路径不存在
 */
egw_err_t    egw_conf_enter(egw_conf_t *cfg, const char *key_path);

/**
 * @brief 查询字符串值
 *
 * key 不存在或类型不匹配时设置 *out = strdup(def)（def 可为 NULL），返回 EGW_ERR_NOTFOUND。
 * *out 是 strdup 的副本，调用方用完必须 free。def 为 NULL 时 *out = NULL。
 *
 * @param[in]  cfg      配置句柄
 * @param[in]  key_path JSON Pointer 路径
 * @param[out] out      写入值指针（strdup 副本，调用方 free）
 * @param[in]  def      默认值（可为 NULL，此时 *out = NULL）
 * @return EGW_OK              成功
 * @return EGW_ERR_INVAL     参数为 NULL
 * @return EGW_ERR_NOTFOUND 键不存在或类型不匹配，*out = strdup(def)
 */
egw_err_t    egw_conf_get_string(egw_conf_t *cfg, const char *key_path, char **out, const char *def);

/**
 * @brief 查询整数值
 *
 * key 不存在或类型不匹配时设置 *out = def，返回 EGW_ERR_NOTFOUND。
 *
 * @param[in]  cfg      配置句柄
 * @param[in]  key_path JSON Pointer 路径
 * @param[out] out      写入值指针
 * @param[in]  def      默认值
 * @return EGW_OK              成功
 * @return EGW_ERR_INVAL     参数为 NULL
 * @return EGW_ERR_NOTFOUND 键不存在或类型不匹配，*out = def
 */
egw_err_t    egw_conf_get_int(egw_conf_t *cfg, const char *key_path, int32_t *out, int32_t def);

/**
 * @brief 查询布尔值
 *
 * key 不存在或类型不匹配时设置 *out = def，返回 EGW_ERR_NOTFOUND。
 *
 * @param[in]  cfg      配置句柄
 * @param[in]  key_path JSON Pointer 路径
 * @param[out] out      写入值指针
 * @param[in]  def      默认值
 * @return EGW_OK              成功
 * @return EGW_ERR_INVAL     参数为 NULL
 * @return EGW_ERR_NOTFOUND 键不存在或类型不匹配，*out = def
 */
egw_err_t    egw_conf_get_bool(egw_conf_t *cfg, const char *key_path, bool *out, bool def);

/**
 * @brief 查询数组长度
 *
 * key 不存在或目标不是数组时设置 *out = def，返回 EGW_ERR_NOTFOUND。
 *
 * @param[in]  cfg      配置句柄
 * @param[in]  key_path JSON Pointer 路径，指向 JSON 数组
 * @param[out] out      写入值指针
 * @param[in]  def      默认值
 * @return EGW_OK              成功
 * @return EGW_ERR_INVAL     参数为 NULL
 * @return EGW_ERR_NOTFOUND 键不存在或目标不是数组，*out = def
 */
egw_err_t    egw_conf_array_length(egw_conf_t *cfg, const char *key_path, int32_t *out, int32_t def);

#endif /* USE_JSON_CONFIG */

#endif /* EGW_CONFIG_H */