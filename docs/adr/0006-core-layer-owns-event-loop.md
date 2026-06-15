# ADR 0006: Core 层持有事件循环，封装 libuv

Core 层对 libuv 做完整封装，成为事件循环（`uv_loop_t`）的唯一所有者。Transport、Protocol 和 App 层通过 Core 层的公共 API 访问 I/O 和定时器功能，不直接调用 libuv。

## 状态

已接受

## 背景

项目已使用 libuv 作为事件循环。当前 Transport 层（`egw_transport_instance_t`）内嵌 `uv_loop_t` 并直接调用 libuv API（`uv_pipe_init`、`uv_read_start` 等）。随着引入 Lua 脚本引擎（ADR 0004）、帧定界超时（Protocol 层需要定时器）以及未来基于协程的调度模型，多个组件都产生了对事件循环的依赖。

多个组件各自持有 libuv 循环引用会导致：
- 各模块需要重复编写 libuv 包装代码
- 定时器生命周期管理不一致
- 切换调度模型（协程/多线程）时被迫修改所有直接调 libuv 的模块

## 决策

1. **Core 层对 libuv 做完整封装**，抽象为公共 API：
   - `egw_loop_t` — 封装 `uv_loop_t`，由 Core 层创建并持有
   - `egw_timer_t` — 封装 `uv_timer_t`，供 Protocol 和 App 层使用
   - `egw_io_handle_t` — 封装 pipe/TCP 等句柄，供 Transport 层使用
   - `egw_signal_t` — 封装 `uv_signal_t`，供 App 层使用

2. **Transport 层不再持有 `uv_loop_t`**，改为通过 Core 层 API 注册 I/O 句柄。`egw_transport_instance_t` 中的 `uv_loop_t` 字段移除，改为引用 `egw_loop_t`。

3. **Protocol 层通过 Core 层的 `egw_timer_t` API 设置超时**，不直接接触 libuv。

4. **App 层调 `egw_loop_run()` 阻塞**，替代当前的 `egw_transport_run()`。

## 理由

- **单一所有权**：事件循环的生命周期由 Core 层统一管理，不存在谁持有、谁释放的歧义
- **接口隔离**：修改底层事件库（libuv → 其他）时只改 Core 层封装，其余模块不变
- **统一超时管理**：Protocol 的帧超时、Lua 脚本的超时、轮询调度器的 one-shot timer 都用同一套 Core API
- **协程/多线程友好**：Core 层可以透明地在 `run_one()` 中添加协程 yield 点或线程调度逻辑，上层无感知

## 后果

- Transport 层需要重构：移除 `uv_loop_t`，改用 Core API 注册 I/O
- 短期内增加 Core 层封装代码，但消除各模块对 libuv 的重复依赖
