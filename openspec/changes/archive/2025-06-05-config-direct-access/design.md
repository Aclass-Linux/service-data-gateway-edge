## Context

当前配置框架采用"parse 时缓存到 struct"模式，模块内部用固定数组承载 JSON 数据。设备数量不可预知时（如 Modbus 从 4 个拓展到 16 个设备）需重新编译。改为使用 handle + key path 直接查询 cJSON 树，不再缓存到 struct。

## Goals / Non-Goals

**Goals:**
- 移除注册表、handler 分发、parse 回调
- 新增 `egw_conf_t *egw_conf_load()` + `egw_conf_free()` 生命周期管理
- 新增 `egw_conf_get_string/int/bool()` 通过 handle + key path 直接查询
- 新增 `egw_conf_array_length()` 获取数组长度
- key path 格式：点号分隔层级，`[n]` 下标访问数组
- 模块不自注册，main() 控制初始化顺序
- 重载 = app 调用 load 建新 handle → 手动释放旧 handle

**Non-Goals:**
- 不处理跨模块配置校验
- 不处理配置写回

## Decisions

### 1. 方法 B：即用即查

放弃缓存到 struct 的方案。模块在运行时通过 key path 实时查询 JSON 树，不预先缓存任何配置数据。JSON 树在 handle 生命周期内常驻内存，退出或重载时释放。

### 2. API 设计

```c
// 底层函数：始终需要默认值
egw_conf_t *egw_conf_load(const char *path);
void        egw_conf_free(egw_conf_t *cfg);

const char *egw_conf_get_string(egw_conf_t *cfg, const char *key_path, const char *def);
int         egw_conf_get_int(egw_conf_t *cfg, const char *key_path, int def);
bool        egw_conf_get_bool(egw_conf_t *cfg, const char *key_path, bool def);
int         egw_conf_array_length(egw_conf_t *cfg, const char *key_path);

// 宏接口：强约束必须传默认值
#define EGW_CONF_STR(cfg, path, def)  egw_conf_get_string((cfg), (path), (def))
#define EGW_CONF_INT(cfg, path, def)  egw_conf_get_int((cfg), (path), (def))
#define EGW_CONF_BOOL(cfg, path, def) egw_conf_get_bool((cfg), (path), (def))
```

### 3. Key path 格式

```
"mqtt.broker"                  → 对象嵌套
"mqtt.auth.username"           → 多层嵌套
"modbus.serial_ports[0].path"  → 数组下标
"modbus.serial_ports[0].devices[2].slave_id"  → 嵌套数组
```

### 4. 生命周期管理

```c
// main()
egw_conf_t *cfg = egw_conf_load("config.json");

// 各模块按 main() 顺序初始化
mqtt_init(cfg);
modbus_init(cfg);

// 重载
egw_conf_t *new_cfg = egw_conf_load("config.json");
egw_conf_free(cfg);
cfg = new_cfg;

// 退出
egw_conf_free(cfg);
```

### 5. 移除项

- `egw_conf_register()`, `EGW_CONF_REGISTER`, `EGW_CONF_MAX`
- `egw_conf_handler_t`, `egw_conf_array_cb_t`
- `egw_conf_array()`, `egw_conf_exists()`, `egw_conf_raw()`
- `egw_conf_cleanup()` → 替换为 `egw_conf_free()`
- parse 回调分发机制

## Risks / Trade-offs

- [注意] 每次取值多一层 key path 解析（点号分割 + 逐层查找），性能可忽略
- [注意] `const char *def` 返回值指向常量，调用者不需要释放
- [注意] `egw_conf_get_int()` 遇非数字字段返回 def，不报错
