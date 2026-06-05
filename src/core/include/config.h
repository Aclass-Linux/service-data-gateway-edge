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

/** @brief 配置句柄（不透明） */
typedef struct egw_conf egw_conf_t;

egw_conf_t  *egw_conf_load(const char *path);
void         egw_conf_free(egw_conf_t *cfg);

const char  *egw_conf_get_string(egw_conf_t *cfg, const char *key_path, const char *def);
int          egw_conf_get_int(egw_conf_t *cfg, const char *key_path, int def);
bool         egw_conf_get_bool(egw_conf_t *cfg, const char *key_path, bool def);
int          egw_conf_array_length(egw_conf_t *cfg, const char *key_path);

#define EGW_CONF_STR(cfg, path, def)    egw_conf_get_string((cfg), (path), (def))
#define EGW_CONF_INT(cfg, path, def)    egw_conf_get_int((cfg), (path), (def))
#define EGW_CONF_BOOL(cfg, path, def)   egw_conf_get_bool((cfg), (path), (def))

#endif /* EGW_CONFIG_H */
