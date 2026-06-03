# Proposal: consolidate-docs-to-openspec

## Why

`docs/` 下的架构和 API 文档是初始化遗留的 C++ 脚手架，与实际项目状态严重脱节。需要审阅后迁移到 `openspec/`，保留六层远景结构，每层标注当前实现状态（已实现/部分实现/未实现）。

## What Changes

- 重构 `architecture.md`：保持六层远景，每层增加实现状态表格
- 重写 `api/core.md`，反映当前 C 代码实际 API
- 将新文档写入 `openspec/specs/`
- 删除 `docs/`

## Impact

| 源文件 | 目标 | 操作 |
|---|---|---|
| `docs/architecture.md` | `openspec/specs/architecture.md` | 重构 + 迁移 |
| `docs/api/core.md` | `openspec/specs/api-core.md` | 重写 + 迁移 |
| `docs/` | — | 删除 |
