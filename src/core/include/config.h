/**
 * @file config.h
 * @brief DataGatewayHub 配置访问层
 *
 * 提供 JSON 配置文件的加载和 key path 实时查询。
 * 模块通过 handle 直接查询 cJSON 树，不缓存到 struct。
 */

#ifndef EGW_CONFIG_H
#define EGW_CONFIG_H

#include "egw_defs.h"
#include <stdbool.h>

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
 * @return EGW_ERR_FILE_NOT_FOUND  文件不存在或无法打开
 * @return EGW_ERR_PARSE       JSON 解析失败或根元素不是对象
 * @return EGW_ERR_HANDLER     参数为 NULL 或内存分配失败
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
 * @brief 查询字符串值
 *
 * @param[in] cfg      配置句柄
 * @param[in] key_path 点分键路径，支持数组下标（如 "modbus.serial_ports[0].path"）
 * @param[in] def      键不存在或类型不匹配时的默认值
 * @return 查询到的字符串指针（指向 cJSON 内部，生命周期与 cfg 相同），或 def
 */
const char  *egw_conf_get_string(egw_conf_t *cfg, const char *key_path, const char *def);

/**
 * @brief 查询整数值
 *
 * @param[in] cfg      配置句柄
 * @param[in] key_path 点分键路径
 * @param[in] def      默认值
 * @return 查询到的整数值，或 def
 */
int          egw_conf_get_int(egw_conf_t *cfg, const char *key_path, int def);

/**
 * @brief 查询布尔值
 *
 * @param[in] cfg      配置句柄
 * @param[in] key_path 点分键路径
 * @param[in] def      默认值
 * @return 查询到的布尔值，或 def
 */
bool         egw_conf_get_bool(egw_conf_t *cfg, const char *key_path, bool def);

/**
 * @brief 查询数组长度
 *
 * @param[in]  cfg      配置句柄
 * @param[in]  key_path 点分键路径，指向一个 JSON 数组
 * @param[out] len      成功时写入数组长度
 * @return EGW_OK            成功
 * @return EGW_ERR_HANDLER   参数为 NULL
 * @return EGW_ERR_MISSING_KEY  键路径不存在
 * @return EGW_ERR_PARSE      目标不是数组
 */
egw_err_t    egw_conf_array_length(egw_conf_t *cfg, const char *key_path, int *len);

/** @brief egw_conf_get_string 的宏包装，自动展开参数 */
#define EGW_CONF_STR(cfg, path, def)    egw_conf_get_string((cfg), (path), (def))
/** @brief egw_conf_get_int 的宏包装 */
#define EGW_CONF_INT(cfg, path, def)    egw_conf_get_int((cfg), (path), (def))
/** @brief egw_conf_get_bool 的宏包装 */
#define EGW_CONF_BOOL(cfg, path, def)   egw_conf_get_bool((cfg), (path), (def))
/** @brief egw_conf_array_length 的宏包装 */
#define EGW_CONF_ARR_LEN(cfg, path, out) egw_conf_array_length((cfg), (path), (out))

#endif /* EGW_CONFIG_H */
