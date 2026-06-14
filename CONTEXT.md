# 项目领域模型

## 术语表

### 错误码 (Error Code)
全局统一的模块故障返回值，类型为 `egw_err_t`（`int32_t`）。`EGW_OK = 0` 表示成功，负值表示错误。顺序编号，新增在末尾追加。错误码按**错误性质**命名（如 `EGW_ERR_NOTFOUND` 表示"不存在"），不体现模块归属。枚举定义通过 X-macro 表 `egw_err.inc` 自动生成，配套 `egw_err_str()` 函数输出 `"EGW_ERR_INVAL (-3): invalid argument"` 格式的字符串。

### 错误码命名 (Error Code Naming)
命名描述错误性质，不体现模块来源。模块来源通过函数名追踪。定义在 `egw_err.inc` 中单点维护，`egw_defs.h` 通过 `#include` 展开为 `enum`，`egw_err.c` 展开为 `egw_err_str()` switch 语句。

### 不透明句柄 (Opaque Handle)
表示模块内部状态的不透明指针类型。命名模式：`egw_{module}_t`（如 `egw_conf_t`），不加 `_handle` 或 `_h` 后缀。生命周期使用领域动词：`load/free`、`connect/disconnect`、`open/close`。与 cJSON 风格一致。

### 代码风格 (Code Style)
遵循 MISRA C:2012 Rule 15.6：所有 `if`/`else`/`while`/`for` 体必须是花括号包裹的复合语句，禁止裸语句。Early return 允许（不强制单出口点），但每个 return 必须有花括号。清理动作独立成行，不与 return 挤在同一行。

### 键路径 (Key Path)
配置查询使用 JSON Pointer（RFC 6901）语法，以 `/` 分隔层级，数组下标直接用数字。如 `/modbus/serial_ports/0/path`。不支持含 `~` 或 `/` 的键名（无需转义）。

### 协程调度器 (Coroutine Scheduler)
用 `egw_coro_sched_t` 表示的无栈协程运行时。内部拥有一个 libuv event loop，负责所有 fd 就绪事件的分发。不提供读写缓冲语义——只做两件事：运行协程循环（`sched_run`），以及当 fd 可读/可写时唤醒等待中的协程。app 通过 `sched_get_loop()` 获取底层 loop 句柄用于信号注册（如 SIGINT）。

### 协程 (Coroutine)
无栈协程，基于 `switch` + `__LINE__` 的 protothread 模式。状态通过 `egw_coro_t` 结构体传入实现可重入。在 I/O 等待点通过 `CORO_YIELD` 让出控制权给调度器，就绪后从断点恢复。协程内禁止一切阻塞操作——所有 fd 必须 `O_NONBLOCK`，等就绪靠 `await_readable`/`await_writable` 显式 yield。

### 传输层 (Transport)
基于协程模型重写的字节流 I/O 通道，不涉及协议解析。每个连接有一个读协程（`await_readable` → `read` → `on_data` → flush 写队列）。写通过内部队列缓冲，协程在每次读后尝试刷写。fd 由主线程同步 open，协程只负责就绪后的 non-blocking 读写。不直接使用任何 libuv API——所有 libuv 交互经协程调度器封装。

### 传输实例 (Transport Instance)
用 `egw_transport_instance_t`（不透明句柄）表示一个完整的 transport 子系统实例。内部包含一个 `egw_coro_sched_t` 调度器。app 负责：
1. 调用 `egw_transport_create()` 创建实例
2. 调用 `egw_serial_register()` 逐一注册端口（主线程同步 open fd + spawn 协程）
3. 调用 `egw_transport_run()`（内部 `sched_run`）阻塞运行
4. 退出时调用 `egw_transport_destroy()` 停止调度器并释放资源

### 传输连接 (Transport Connection)
用 `egw_serial_t *`（不透明句柄）表示一个已注册的串口连接。由 `egw_serial_register` 创建，内部包含一个 `egw_coro_t` 运行读循环。关闭后句柄不可再用。

### 协议层 (Protocol)
负责解析数据的语义和帧边界检测。例如 Modbus RTU 帧解析、MQTT 报文编解码。运行在协程调度器上，是一类协程，与传输层共享同一调度器。当前为占位函数，待后续实现。

### 数据包 (Packet)
Transport 和 Protocol 之间传递消息的载体，参考 FFmpeg AVPacket 设计。引用计数 + 对象池，指针传递零拷贝。字段包括：buffer 指针、数据长度、来源标识（整型 ID）、调试追踪序号（递增 uint32_t）、时间戳。本阶段暂不实现，等 Protocol 层时再定。

### 通道 (Channel)
Transport 和 Protocol 之间的双队列连接，包含 rx_queue（Transport → Protocol）和 tx_queue（Protocol → Transport）。对外暴露为一个 egw_channel_t 句柄，两个队列各自独立加锁和唤醒。本阶段暂不实现，等 Protocol 层时再定。

---

## 待办

- **编译期状态机（Protothread）**：已在 core 中实现为 `egw_coro` 模块，宏提供 `CORO_BEGIN`/`CORO_YIELD`/`CORO_END`，状态通过 `egw_coro_t` 结构体传入，支持多实例并发（可重入）。
- **协议层协程**：将协议逻辑也写成协程，挂载在同一调度器上。与传输层之间通过 Channel 通信。
- **协程写队列**：当前 transport 写通过内部队列缓冲 + 读循环内刷写。后续协议层协程接入后，需要独立唤醒机制（写入队后能主动唤醒刷写协程）。
- **线程池集成**：如果未来出现加密解密等计算密集型任务，通过 `coro_submit_to_threadpool` + eventfd 唤醒协程，不阻塞调度器。
