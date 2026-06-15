# 实施记录

## 2026-06-15：Core 事件循环 + App FSM + Protocol FSM

### egw_loop_t 扩展（ADR-0006）

原有 `egw_loop_t`（create/run/stop/destroy）扩展为完整的 libuv 封装：

- `egw_poll_t`：fd 就绪通知（替代 `uv_poll_t`）
- `egw_timer_t`：定时器（替代 `uv_timer_t`）
- `egw_signal_t`：信号处理（替代 `uv_signal_t`）

结构体公开在 `egw_loop.h` 中，调用者可嵌入。API 通过回调函数指针注册，内部用 libuv wrapper 转发。

`egw_fsm.h` 中 `egw_signal_t`（事件信号类型）重命名为 `egw_sig_t` 避免冲突。

修复：`egw_loop_run()` 在 `uv_stop()` 后误报 `EGW_ERR_LOOP_RUN` → 改为检查 `should_stop` 标志。

### Protocol FSM：Modbus RTU 帧解析

`egw_proto_ctx_t` 每端口独立上下文。`egw_proto_feed(ctx, data, len)` 推入字节，内部基于功能码判定期望帧长度 + CRC-16 校验，同步返回：

| 返回值 | 含义 |
|--------|------|
| `EGW_PROTO_FRAME_READY` | 完整帧就绪，`egw_proto_get_frame()` 获取 |
| `EGW_PROTO_NEED_MORE` | 等待更多字节 |
| `EGW_PROTO_FRAME_ERROR` | 帧异常，内部已自动重置 |

### App FSM：状态机驱动的采集调度

`gateway_app.c` 以 `egw_fsm_t` 驱动，两状态：`st_running` / `st_shutdown`。采集通过定时器调度 → 发 Modbus 请求 → poll 等响应 → Protocol 解析 → 总线发布。错误自动 reopen。

### 新增文件

| 文件 | 用途 |
|------|------|
| `src/core/include/egw_ctype.inc` | 9 种核心类型 X-macro |
| `src/core/include/egw_ptable.h` | 点表二进制格式定义 |
| `src/core/include/egw_bus.h` | Pub/Sub 总线 API |
| `src/core/egw_bus.c` | 总线实现（数组订阅表） |
| `src/core/include/egw_runtime.h` | 运行时单例 API |
| `src/core/egw_runtime.c` | 运行时实现（聚合 loop+bus） |
| `src/ptable/` | 点表 mmap 加载器模块 |
| `src/persist/` | 运行时值持久化模块 |

### 设计决策

- 协程方案放弃，用 `egw_fsm_t` 状态机替代
- Transport 层定为纯 I/O 工具层，不持调度器/回调/事件循环
- `egw_loop_t` 提供 poll/timer/signal 最小封装
- 配置查询路径修正为 `"/modbus/serial_ports/N/path"`
- `egw_value_t` 8 字节无判别式 union + seqlock 持久化槽位
