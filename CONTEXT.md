# 项目领域模型

## 术语表

### 错误码 (Error Code)
全局统一的模块故障返回值，类型为 `egw_err_t`（`int32_t`）。`EGW_OK = 0` 表示成功，负值表示错误。顺序编号，新增在末尾追加。模块归属通过宏命名前缀区分（如 `EGW_ERR_MQTT_*`、`EGW_ERR_MODBUS_*`），不按值域分段。

### 错误码命名前缀 (Error Code Name Prefix)
宏名称中的模块标识部分，用于在代码中直观区分错误来源。前缀约定：通用错误无模块前缀（`EGW_ERR_*`），模块错误带模块前缀（`EGW_ERR_MQTT_*`、`EGW_ERR_MODBUS_*`）。

### 不透明句柄 (Opaque Handle)
表示模块内部状态的不透明指针类型。命名模式：`egw_{module}_t`（如 `egw_conf_t`），不加 `_handle` 或 `_h` 后缀。生命周期使用领域动词：`load/free`、`connect/disconnect`、`open/close`。与 cJSON 风格一致。

### 代码风格 (Code Style)
遵循 MISRA C:2012 Rule 15.6：所有 `if`/`else`/`while`/`for` 体必须是花括号包裹的复合语句，禁止裸语句。Early return 允许（不强制单出口点），但每个 return 必须有花括号。清理动作独立成行，不与 return 挤在同一行。

### 键路径 (Key Path)
配置查询使用 JSON Pointer（RFC 6901）语法，以 `/` 分隔层级，数组下标直接用数字。如 `/modbus/serial_ports/0/path`。不支持含 `~` 或 `/` 的键名（无需转义）。