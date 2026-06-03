## Why

M1 第 1 周需要实现模拟数据源 `simulator`，作为网关的数据采集上游。当前项目仅有 hello world 测试桩，没有可用的数据输入路径。

## What Changes

- 在 `src/app/` 下创建模块化的数据源程序 `simulator`
- 实现随机温度值生成模块
- 使用有名管道（FIFO）向外部输出数据流
- 使用匿名管道在模块间传递数据
- 处理 SIGPIPE 信号防止写出错时崩溃
- 更新 `src/app/CMakeLists.txt` 添加 simulator 构建目标

## Capabilities

### New Capabilities
- `simulator-fifo`: 模拟数据源，每秒生成随机温度值（20-100），写入命名管道 `/tmp/temp_fifo`

### Modified Capabilities

<!-- 无 -->

## Impact

- `src/app/` — 新增模块文件：`simulator.c`（主程序）、`data_gen.c`/`data_gen.h`（温度生成模块）、`fifo_writer.c`/`fifo_writer.h`（FIFO 写入模块）、`pipe_util.c`/`pipe_util.h`（匿名管道工具）
- `src/app/CMakeLists.txt` — 新增 `simulator` 可执行目标
