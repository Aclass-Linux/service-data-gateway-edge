## Why

当前配置框架采用"parse 时缓存到 struct"模式，模块内部用固定数组承载 JSON 数据（如 `devices[4]`）。设备数量动态变化时（如 4 个变 16 个）需要重新编译。改为"即用即查"模式：不缓存到 struct，通过 handle + key path 直接查询 cJSON 树，不再受 struct 大小限制。

## What Changes

- **BREAKING**: 移除模块注册表、handler 分发、parse 回调机制
- **BREAKING**: 移除 `egw_conf_register()`、`EGW_CONF_REGISTER`、`EGW_CONF_MAX`
- 新增 `egw_conf_t *egw_conf_load(const char *path)` → 返回 handle
- 新增 `egw_conf_get_string/cfg, key_path, def)` → 通过 key path 取值
- 新增 `egw_conf_get_int/bool`、`egw_conf_array_length`
- 新增 `egw_conf_free(cfg)` → 释放整个树
- 模块不再自注册，main() 控制初始化顺序

## Capabilities

### New Capabilities
- `config-direct-access`: 通过 handle + key path 直接查询 JSON 树的配置访问层

### Modified Capabilities
- (空)

## Impact

- `src/core/config.c`：全部重写，删除注册表、handler 分发，实现 key path 解析
- `src/core/config.h`：API 完全变更，删除旧类型和宏
- `src/core/CMakeLists.txt`：可能简化
- `src/app/main.c`：改为使用新 API
- `src/app/demo_module.c`：删除或重写为直接查询
- `src/core/egw_defs.h`：`EGW_EXPORT` 保留，其他配置相关前向声明移除
