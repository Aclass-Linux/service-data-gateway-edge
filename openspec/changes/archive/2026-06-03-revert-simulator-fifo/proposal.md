## Why

现有 simulator 使用 fork + 匿名管道 + FIFO 的模型存在设计问题。重新实现计划采用多线程 + 锁 + 共享内存方案（参考 FFmpeg 的线程模型），因此需要先清理现有代码和规格。

## What Changes

- **BREAKING**: 删除 `src/app/simulator.c` 及关联模块（`data_gen`、`pipe_util`、`fifo_writer`）
- **BREAKING**: 移除 `openspec/specs/simulator-fifo/` 主规格
- 清理 `src/app/CMakeLists.txt` 中 simulator 构建目标
- 清理 `src/app/LEARN.md` 中相关记录

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `simulator-fifo`: REMOVED — 整个 capability 废弃，待后续重新设计

## Impact

- `src/app/simulator.c` — 删除
- `src/app/data_gen.h` / `src/app/data_gen.c` — 删除
- `src/app/pipe_util.h` / `src/app/pipe_util.c` — 删除
- `src/app/fifo_writer.h` / `src/app/fifo_writer.c` — 删除
- `src/app/CMakeLists.txt` — 移除 simulator 目标
- `src/app/LEARN.md` — 清理相关章节
- `openspec/specs/simulator-fifo/spec.md` — 删除
