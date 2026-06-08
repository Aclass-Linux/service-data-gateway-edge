# ADR 0002: 配置键路径使用 JSON Pointer（RFC 6901）

## 状态

已接受

## 背景

配置查询需要一种路径语法来定位 JSON 树中的节点。初始实现使用自定义的点分+数组下标语法（`modbus.serial_ports[0].path`），由手工编写的字符串解析器 `key_path_resolve` 实现（约 65 行代码）。

项目已依赖 cJSON 子模块，其中包含 cJSON_Utils，提供标准 JSON Pointer 实现 `cJSONUtils_GetPointer`。

## 决策

采用 JSON Pointer（RFC 6901）作为配置键路径语法，替换自定义点分语法。删除 `key_path_resolve`，改用 `cJSONUtils_GetPointer`。

路径示例变更：
- 旧：`"modbus.serial_ports[0].path"`
- 新：`"/modbus/serial_ports/0/path"`

约束：JSON 配置文件的键名不得包含 `~` 或 `/` 字符，因此无需实现 RFC 6901 的转义规则（`~0`、`~1`）。

## 理由

- **消除自定义解析器**：65 行手工字符串解析代码被删除，减少 bug 面和维护成本
- **标准语法**：JSON Pointer 是 IETF 标准（RFC 6901），工具生态支持广泛，调试时可直接对照 RFC
- **零额外依赖**：cJSON_Utils 已在 third-party 子模块中，无需引入新库
- **嵌入式适用**：JSON Pointer 的 `/` 分隔符无需转义常见键名，比点分+方括号语法更简洁