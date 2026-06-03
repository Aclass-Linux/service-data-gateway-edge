# Design: consolidate-docs-to-openspec

## Context

项目处于 M1 第 0 周，仅有 `src/app/main.c`（hello world）。`docs/` 下有两份遗留文档，引用不存在的 C++  API，与实际严重脱节。需要审阅后迁移到 `openspec/specs/` 并删除 `docs/`。

## Goals / Non-Goals

**Goals:**
- 保留六层远景框架，按实际进度标注状态
- 当前仅维护 `app`、`protocol`、`connectors`、`core` 四层定义
- 将现有 API（当前仅 `main()`）写入文档
- 不存在的 API 标注为"未实现"
- 删除 `docs/`

**Non-Goals:**
- 不新增 `hub`、`data` 等尚未规划的分层定义
- 不修改任何源代码
- 不修改构建系统

## Decisions

### Decision 1: 架构四层优先

参照 `项目计划.md` 的 M1 规划，当前阶段实际涉及的分层只有：

| 层 | 职责 | 状态 |
|---|---|---|
| `app` | 可执行入口（main） | 部分实现（hello world） |
| `protocol` | 协议解析（Modbus、IEC 104、自定义） | 未实现 |
| `connectors` | 外部适配器（MQTT、Modbus TCP 等） | 未实现 |
| `core` | 基础组件（日志、配置、错误码） | 未实现 |

`hub` 和 `data` 层待后续规划时补充，架构文档预留扩展位置。

### Decision 2: API 文档如实反映现状

`docs/api/core.md` 引用了 `DataGatewayHub::Core::Logger` 等不存在的 API，新文档按现有代码如实列出 `main()`，其余标注"未实现"。

### Decision 3: 迁移后删除源目录

所有文档写入 `openspec/specs/` 后，删除整个 `docs/` 目录。

## Affected Areas

- `docs/architecture.md` → `openspec/specs/architecture.md`（重构）
- `docs/api/core.md` → `openspec/specs/api-core.md`（重写）
- `docs/` → 删除
