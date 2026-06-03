## Context

当前仅有一个 hello world 主程序 `src/app/main.c`。M1 第 1 周需要实现模拟数据源 `simulator`，作为网关的采集上游。该程序需模块化设计，同时使用匿名管道（模块间解耦）和有名管道 FIFO（进程间输出）。

## Goals / Non-Goals

**Goals:**
- 模块化：温度生成、匿名管道通信、FIFO 写入独立为编译单元
- 数据通过匿名管道从生成模块流向写入模块，而非直接函数调用
- SIGPIPE 信号处理，写入 FIFO 无读端时不崩溃
- `simulator` 作为独立可执行程序，由 CMake 构建

**Non-Goals:**
- 不实现数据持久化
- 不实现网关主控（gateway_v1 在第 2 周实现）
- 不实现 MQTT 上传（第 4 周）

## Decisions

### Decision 1: 模块拆分

按职责拆为三个模块 + 一个主入口：

| 模块 | 文件 | 职责 |
|---|---|---|
| 主程序 | `simulator.c` | 组装各模块，主循环 |
| 数据生成 | `data_gen.h` / `data_gen.c` | 生成随机温度值（20-100） |
| 匿名管道 | `pipe_util.h` / `pipe_util.c` | 封装 pipe() 通信，读写分离 |
| FIFO 写入 | `fifo_writer.h` / `fifo_writer.c` | 管理命名管道生命周期，写入数据 |

理由：每个模块职责单一，可独立测试和替换。

### Decision 2: 数据流

```
data_gen → [匿名管道] → fifo_writer → [FIFO /tmp/temp_fifo] → 外部进程
```

- `data_gen` 单方面向匿名管道写端写入温度值
- `fifo_writer` 从匿名管道读端读取并写入 `/tmp/temp_fifo`
- 主程序 `simulator.c` fork 或使用线程驱动两端

### Decision 3: 进程模型

使用 `fork()` 创建子进程：
- **子进程**：运行 `data_gen`，每 1 秒生成一个温度，写入匿名管道写端
- **父进程**：运行 `fifo_writer`，从匿名管道读端读取，写入 `/tmp/temp_fifo`，处理 SIGPIPE

理由：匿名管道的自然用途是进程间通信，fork 模型比线程更能体现教学目的（项目计划明确要求）。

### Decision 4: FIFO 管理

- 程序启动时调用 `mkfifo()` 创建（若不存在），权限 0666
- 以只写方式 `open()` 打开 FIFO — 默认阻塞直到读端打开
- SIGPIPE 通过 `signal(SIGPIPE, SIG_IGN)` 忽略或自定义处理函数

## Risks / Trade-offs

- [FIFO 阻塞] FIFO 以只写方式打开时默认阻塞直到有读端 → 在当前版本中接受，第 2 周实现读端后自然解决
- [随机性] `rand()` 非线程安全但当前为单进程 fork，无竞态风险
