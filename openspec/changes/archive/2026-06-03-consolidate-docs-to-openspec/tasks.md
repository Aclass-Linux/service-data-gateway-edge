# Tasks: consolidate-docs-to-openspec

## 1. 架构文档

- [x] 1.1 审阅 `docs/architecture.md`，确定四层结构（app / protocol / connectors / core）
- [x] 1.2 编写架构文档到 `openspec/specs/architecture.md`
- [x] 1.3 每层标注实现状态（已实现 / 部分实现 / 未实现）

## 2. API 文档

- [x] 2.1 审阅 `docs/api/core.md`
- [x] 2.2 编写当前真实 API 到 `openspec/specs/api-core.md`
- [x] 2.3 不存在的 API 标注为"未实现"

## 3. 清理

- [x] 3.1 删除 `docs/` 目录
- [x] 3.2 更新 `AGENTS.md` 中引用 `docs/` 的链接

## 4. 验证

- [x] 4.1 确认 `docs/` 已删除
- [x] 4.2 确认 `openspec/specs/architecture.md` 和 `api-core.md` 内容正确
- [x] 4.3 确认构建不受影响
