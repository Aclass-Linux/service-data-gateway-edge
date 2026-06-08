# Core API

## 说明

本文档列出当前项目中实际存在的 API。不存在的 API 标注为"未实现"。

---

## 已实现

### `main` — 程序入口

```c
int main(int argc, char *argv[]);
```

- **文件**：`src/app/main.c`
- **说明**：加载 config.json（-c 参数可指定路径），打印 MQTT/Modbus 配置
- **状态**：已实现

---

## 未实现

以下 API 在遗留文档中被引用，但项目当前版本中不存在。作为远期规划保留。

### Logger — 日志入口

```cpp
DataGatewayHub::Core::Logger
```

- **说明**：日志记录封装
- **状态**：未实现（M2+ 规划）

### Config — 配置存储

```c
egw_conf_t *egw_conf_load(const char *path);
void        egw_conf_free(egw_conf_t *cfg);
const char *egw_conf_get_string(egw_conf_t *cfg, const char *key_path, const char *def);
int         egw_conf_get_int(egw_conf_t *cfg, const char *key_path, int def);
bool        egw_conf_get_bool(egw_conf_t *cfg, const char *key_path, bool def);
int         egw_conf_array_length(egw_conf_t *cfg, const char *key_path);
```

- **文件**：`src/core/config.c` + `src/core/include/config.h`
- **说明**：cJSON 封装，支持 key path 查询（如 `"modbus.serial_ports[0].baud"`），不足时返回默认值
- **状态**：已实现

### egw_err_t — 错误码定义

```c
typedef int32_t egw_err_t;
#define EGW_OK                  0
#define EGW_ERR_FILE_NOT_FOUND  (-1)
#define EGW_ERR_PARSE           (-2)
#define EGW_ERR_MISSING_KEY     (-3)
#define EGW_ERR_REGISTRY_FULL   (-4)
#define EGW_ERR_HANDLER         (-5)
```

- **文件**：`src/core/include/egw_defs.h`
- **说明**：全项目共享错误码，新增追加到末尾保持兼容
- **状态**：已实现
