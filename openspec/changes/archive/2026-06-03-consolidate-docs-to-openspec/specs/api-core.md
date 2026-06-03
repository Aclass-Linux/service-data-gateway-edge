# API Core Spec

## ADDED Requirements

### Requirement: CORE-001 — 当前 API 清单

`api-core.md` 应列出 `src/app/` 和 `src/core/`（如有）中当前真实存在的公开 API。

#### Scenario: API 条目格式

- **WHEN** 读者查看任意 API 条目
- **THEN** 该条目包含：函数签名、所属文件、简要说明
- **AND** 每个 API 必须有"实现状态"标注（已实现 / 未实现）

#### Scenario: 当前 API 内容

- **WHEN** 读者打开 `api-core.md`
- **THEN** 文档至少包含 `main()` 函数的条目（`src/app/main.c`）
- **AND** `main()` 状态标注为"已实现"
- **AND** 其余引用的 API（如 Logger、Config 等）状态标注为"未实现"

### Requirement: CORE-002 — 不存在的 API 处理

遗留文档中引用了项目中不存在的 API（如 `DataGatewayHub::Core::Logger`），在新文档中必须标注为"未实现"，不可直接删除或忽略。
