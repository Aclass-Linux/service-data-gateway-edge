## ADDED Requirements

### Requirement: 配置框架入口

系统 SHALL 提供 `egw_conf_load()` 函数，接受配置文件路径，加载 JSON 文件并分发到各模块。

#### Scenario: 正常加载
- **WHEN** 调用 `egw_conf_load("config.json")`
- **THEN** 文件被成功解析为 cJSON 对象
- **AND** 顶层 key 被逐一匹配到已注册的模块 handler

#### Scenario: 文件不存在
- **WHEN** 配置文件路径无效
- **THEN** 返回错误码 EGW_ERR_FILE_NOT_FOUND

#### Scenario: JSON 格式错误
- **WHEN** 文件内容不是合法 JSON
- **THEN** 返回错误码 EGW_ERR_PARSE

### Requirement: 模块自注册

各模块 SHALL 通过 `egw_conf_register()` 注册自己的配置 key 和 handler。

#### Scenario: 注册 key
- **WHEN** 调用 `egw_conf_register("mqtt", mqtt_parse)`
- **THEN** 框架将该 key 加入注册表
- **AND** 配置加载时该 key 对应的 JSON 段会传给 `mqtt_parse`

#### Scenario: 未知 key 忽略
- **WHEN** JSON 配置中存在未注册的顶层 key
- **THEN** 该 key 被忽略，不报错

### Requirement: 取值封装

框架 SHALL 提供 `egw_conf_string()`、`egw_conf_int()`、`egw_conf_bool()` 三个取值函数，屏蔽底层 cJSON 操作。

#### Scenario: 取字符串
- **WHEN** 调用 `egw_conf_string(conf, "broker")`
- **THEN** 返回对应字段的字符串指针
- **AND** 字段不存在或非字符串时返回 NULL

#### Scenario: 取整数
- **WHEN** 调用 `egw_conf_int(conf, "port", 1883)`
- **THEN** 返回对应字段的整数值
- **AND** 字段不存在时返回默认值 1883

#### Scenario: 取布尔值
- **WHEN** 调用 `egw_conf_bool(conf, "daemon", false)`
- **THEN** 返回对应字段的布尔值
- **AND** 字段不存在时返回默认值 false

### Requirement: 模块配置存储

各模块 SHALL 在内部以 static 变量持有配置，对外提供 const getter 函数。

#### Scenario: 配置读取
- **WHEN** 模块配置解析完成
- **THEN** 其他模块通过 `mqtt_get_config()` 等 getter 获取配置
- **AND** 返回的指针指向 const struct，只读

#### Scenario: 重载支持
- **WHEN** `mqtt_parse()` 被第二次调用
- **THEN** 先清空静态变量中的旧值
- **AND** 重新填充新值

### Requirement: 自动注册

模块 SHALL 通过初始化函数或编译期机制自动完成注册。

#### Scenario: 自动注册流程
- **WHEN** `main()` 启动
- **THEN** 各模块的初始化函数自动调用 `egw_conf_register()`
- **AND** 不需要手动维护注册表

### Requirement: 循环数组遍历

框架 SHALL 提供 `egw_conf_array()` 函数，支持模块遍历 JSON 数组并回调处理每个元素。

#### Scenario: 遍历数组
- **WHEN** 调用 `egw_conf_array(conf, "serial_ports", port_handler, &ctx)`
- **THEN** 对数组中每个元素调用 `port_handler`
- **AND** 每个元素作为新的 `egw_conf_t*` 传入回调
