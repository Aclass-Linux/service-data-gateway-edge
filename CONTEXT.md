# 项目领域模型

## 术语表

### 错误码 (Error Code)
全局统一的模块故障返回值，类型为 `egw_err_t`（`int32_t`）。`EGW_OK = 0` 表示成功，负值表示错误。顺序编号，新增在末尾追加。错误码按**错误性质**命名（如 `EGW_ERR_NOTFOUND` 表示"不存在"），不体现模块归属。枚举定义通过 X-macro 表 `egw_err.inc` 自动生成，配套 `egw_err_str()` 函数输出 `"EGW_ERR_INVAL (-3): invalid argument"` 格式的字符串。

### 错误码命名 (Error Code Naming)
命名描述错误性质，不体现模块来源。模块来源通过函数名追踪。定义在 `egw_err.inc` 中单点维护，`egw_defs.h` 通过 `#include` 展开为 `enum`，`egw_err.c` 展开为 `egw_err_str()` switch 语句。

### 南向点表 (Southbound Point Table)
每个 Modbus 设备的测点协议清单，以 `.bin` 二进制文件存储，通过 `mmap` 零拷贝访问。描述如何在线缆上解析字节以及本侧的值处理：功能码、寄存器起始地址、数量、南向交互类型（uint16/int32/float 等，决定怎么把 Modbus 寄存器字节解析为原始值）、采集周期（毫秒）、是否启用，以及**本侧转换系数**（scale/offset，带「是否需要转换」标志位）和**可选的本侧死区**（deadband）。每设备一个独立文件，离线构建、运维推送。

转换系数与死区遵循「谁用谁存」：南向自己持有，不集中在路由表。sig_id 的索引与对齐统一归路由表（见 [[Routing Table]]）。

### 北向点表 (Northbound Point Table)
网关作为 Modbus 从设备的数据模型定义文件。描述如何把核心类型值编码到北向协议以及本侧的值处理：北向目标类型、**通道标识（位掩码：MQTT/SQLite/Lua...）**、MQTT topic、寄存器地址映射，以及**本侧转换系数**（scale/offset）和**可选的本侧死区**（deadband）。离线构建。

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

路由表**不存**转换系数（scale/offset）和死区（deadband）——这些是「谁用谁存」，由南北向各自持有。查找采用完美哈希或等价 O(1) 结构。参考 ADR-0005。

路由表**不存**转换系数（scale/offset）和死区（deadband）——这些是「谁用谁存」，由南北向各自持有。查找采用完美哈希或等价 O(1) 结构。参考 ADR-0005。

---

## 原有术语（已确认，继续有效）

- **编译期状态机（Protothread）**：已在 core 中实现为 `egw_coro` 模块，宏提供 `CORO_BEGIN`/`CORO_YIELD`/`CORO_END`，状态通过 `egw_coro_t` 结构体传入，支持多实例并发（可重入）。
- **协议层协程**：将协议逻辑也写成协程，挂载在同一调度器上。与传输层之间通过 Channel 通信。
- **协程写队列**：当前 transport 写通过内部队列缓冲 + 读循环内刷写。后续协议层协程接入后，需要独立唤醒机制（写入队后能主动唤醒刷写协程）。
- **线程池集成**：如果未来出现加密解密等计算密集型任务，通过 `coro_submit_to_threadpool` + eventfd 唤醒协程，不阻塞调度器。
