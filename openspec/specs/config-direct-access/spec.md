## ADDED Requirements

### Requirement: 配置加载生命周期

系统 SHALL 提供 `egw_conf_load()` 和 `egw_conf_free()` 管理配置句柄的生命周期。

#### Scenario: 正常加载
- **WHEN** 调用 `egw_conf_load("config.json")`
- **THEN** 返回非空句柄 `egw_conf_t*`
- **AND** JSON 文件被解析并常驻内存

#### Scenario: 文件不存在
- **WHEN** 文件路径无效
- **THEN** 返回 NULL

#### Scenario: JSON 格式错误
- **WHEN** 文件内容不是合法 JSON
- **THEN** 返回 NULL

#### Scenario: 释放句柄
- **WHEN** 调用 `egw_conf_free(cfg)`
- **THEN** 释放该句柄关联的所有内存

### Requirement: key path 查询

系统 SHALL 支持点号分隔的 key path 查询 JSON 嵌套值。取值接口 SHALL 以宏形式提供，必须传入默认值。

#### Scenario: 取字符串（宏）
- **WHEN** 调用 `EGW_CONF_STR(cfg, "mqtt.broker", "default_broker")`
- **THEN** 返回对应字段的字符串值
- **AND** 字段不存在时返回默认值 `"default_broker"`

#### Scenario: 取整数（宏）
- **WHEN** 调用 `EGW_CONF_INT(cfg, "mqtt.port", 1883)`
- **THEN** 返回对应字段的整数值
- **AND** 字段不存在或非数字时返回默认值 1883

#### Scenario: 取布尔值（宏）
- **WHEN** 调用 `EGW_CONF_BOOL(cfg, "watchdog.enabled", true)`
- **THEN** 返回对应字段的布尔值
- **AND** 字段不存在或非布尔时返回默认值 true

### Requirement: 数组访问

系统 SHALL 支持通过 `[下标]` 语法访问 JSON 数组元素，并提供数组长度查询。

#### Scenario: 数组元素
- **WHEN** 调用 `EGW_CONF_STR(cfg, "modbus.serial_ports[0].path", "/dev/default")`
- **THEN** 返回数组中第 0 个元素的 path 字段

#### Scenario: 数组长度
- **WHEN** 调用 `egw_conf_array_length(cfg, "modbus.serial_ports")`
- **THEN** 返回数组的元素个数
- **AND** 字段不存在或非数组时返回 0

### Requirement: 重载支持

系统 SHALL 支持通过重复调用 load/free 实现配置重载，不依赖全局状态。

#### Scenario: 热重载
- **WHEN** 调用 `egw_conf_load()` 得到新句柄
- **AND** 调用 `egw_conf_free()` 释放旧句柄
- **THEN** 旧句柄对应的内存完全释放
- **AND** 新句柄可正常查询
