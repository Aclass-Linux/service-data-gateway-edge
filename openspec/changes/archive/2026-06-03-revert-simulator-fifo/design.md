## Context

`week1-simulator-fifo` 实现了基于 fork + 匿名管道 + FIFO 的数据源模拟器。该方案在设计评审中发现问题：
- fork 模型下父/子进程通过匿名管道传递 double 值的方案过于复杂
- FIFO 阻塞 open 依赖外部读端，不利于自动化测试
- SIGPIPE 信号处理增加了不必要的复杂度

决定回退到干净状态，改用多线程 + 共享内存方案重新实现（参考 FFmpeg 的线程模型）。

## Goals / Non-Goals

**Goals:**
- 删除 `src/app/` 下所有 simulator 相关源文件和模块
- 从 `openspec/specs/` 移除 `simulator-fifo` capability
- 保持 `gateway` 目标不受影响
- 保持学习记录 `journal/` 和归档不变

**Non-Goals:**
- 不实现新的数据源（下次 change 再做）
- 不修改 `openspec/specs/architecture.md` 等已有规格

## Decisions

### Decision 1: 只删代码，保留学习记录

`journal/` 和 `openspec/changes/archive/` 中的文件保留不动——它们是学习历史，不应随代码回退而丢失。

### Decision 2: CMakeLists.txt 只移除 simulator 目标

不修改 `gateway` 目标的构建配置，只移除新增的 `simulator` 和关联 install。

## Risks / Trade-offs

- [已归档变更] `week1-simulator-fifo` 已归档，其历史可在 `openspec/changes/archive/` 查阅，不受影响
