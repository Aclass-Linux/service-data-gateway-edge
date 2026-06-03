# Architecture Spec

## ADDED Requirements

### Requirement: ARCH-001 — 六层远景架构描述

系统架构按六层定义：`app → hub → connectors → data → protocol → core`，链接方向自顶向下（app 依赖 hub，hub 依赖 connectors 和 data，以此类推至 core）。

#### Scenario: 架构总览表

- **WHEN** 读者查看架构文档
- **THEN** 文档顶部展示一个表格列出全部六层
- **AND** 每层包含：层级名称、职责描述、当前实现状态

#### Scenario: 实现状态标注

- **WHEN** 文档提及某一层
- **THEN** 该层条目必须包含实现状态字段
- **AND** 状态字段为以下三者之一：
  - **已实现** — 该层有可编译的源代码
  - **部分实现** — 该层有部分代码或桩代码
  - **未实现** — 该层仅有目录结构，无任何代码
- **AND** 状态为"未实现"时，标注 `（未实现）` 字样

### Requirement: ARCH-002 — M1 当前范围说明

架构文档需明确标注当前阶段（M1）实际涉及的范围，让读者理解六层中哪些与当前工作相关。

#### Scenario: M1 范围划定

- **WHEN** 读者阅读架构文档
- **THEN** 文档包含一节说明 M1 阶段实际覆盖的层
- **AND** M1 阶段仅涉及 `app` 层，其余层为 M2+ 规划
