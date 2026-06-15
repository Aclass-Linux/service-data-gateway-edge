# 项目领域模型

## 术语表

### 错误码 (Error Code)
全局统一的模块故障返回值，类型为 `egw_err_t`（`int32_t`）。`EGW_OK = 0` 表示成功，负值表示错误。顺序编号，新增在末尾追加。错误码按**错误性质**命名（如 `EGW_ERR_NOTFOUND` 表示"不存在"），不体现模块归属。枚举定义通过 X-macro 表 `egw_err.inc` 自动生成，配套 `egw_err_str()` 函数输出 `"EGW_ERR_INVALID_ARG (-3): invalid argument"` 格式的字符串。

### 错误码命名 (Error Code Naming)
命名描述错误性质，不体现模块来源。模块来源通过函数名追踪。定义在 `egw_err.inc` 中单点维护，`egw_defs.h` 通过 `#include` 展开为 `enum`，`egw_err.c` 展开为 `egw_err_str()` switch 语句。

### 南向点表 (Southbound Point Table)
每个 Modbus 设备的测点协议清单，以 `.bin` 二进制文件存储，通过 `mmap` 零拷贝访问。描述如何在线缆上解析字节以及本侧的值处理：功能码、寄存器起始地址、数量、南向交互类型（uint16/int32/float 等，决定怎么把 Modbus 寄存器字节解析为原始值）、采集周期（毫秒）、是否启用，以及**本侧转换系数**（scale/offset，带「是否需要转换」标志位）和**可选的本侧死区**（deadband）。每设备一个独立文件，离线构建、运维推送。

转换系数与死区遵循「谁用谁存」：南向自己持有，不集中在路由表。sig_id 的索引与对齐统一归路由表（见 [[Routing Table]]）。

### 北向点表 (Northbound Point Table)
网关作为 Modbus 从设备的数据模型定义文件。描述如何把核心类型值编码到北向协议以及本侧的值处理：北向目标类型、**通道标识（位掩码：MQTT/SQLite/Lua...）**、MQTT topic、寄存器地址映射，以及**本侧转换系数**（scale/offset）和**可选的本侧死区**（deadband）。离线构建。

### 传输层 (Transport)
纯同步非阻塞 I/O 工具层，负责端口（串口/TCP）的 open/read/write/flush/close，不涉及协议解析。**不持有事件循环或任何句柄、不注册回调、不运行状态机**——只提供 fd 级别的字节流读写接口。App 层负责编排：通过 `egw_loop_t` 注册 fd 就绪 → 调用 transport read → 将字节喂入 Protocol FSM → 处理完整帧。当前实现：`egw_serial.c`。

### 传输连接 (Transport Connection)
用 `egw_serial_t *`（不透明句柄）表示一个已打开的串口连接。由 `egw_serial_open()` 同步创建（open fd + termios 配置），内部持有 fd、参数副本和写缓冲区。关闭后句柄不可再用。

### 协议层 (Protocol)
负责解析数据的语义和帧边界检测。以状态机驱动，每个连接独立一个 `egw_proto_ctx_t` 上下文。App 通过 `egw_proto_feed()` 推入原始字节，内部做帧定界 + CRC 校验，同步返回 FRAME_READY/NEED_MORE/FRAME_ERROR。当前实现 Modbus RTU 帧解析（`src/protocol/egw_protocol.c`）。

### 路由表 (Routing Table)
路由表**不存**转换系数（scale/offset）和死区（deadband）——这些是「谁用谁存」，由南北向各自持有。查找采用完美哈希或等价 O(1) 结构。参考 DS-005。

### 总线值 (Bus Value)
用 `egw_value_t` 表示的无判别式 8 字节 union（`b/i16/u16/i32/u32/i64/u64/f32/f64/raw`）。`raw` 成员与持久化 `_Atomic uint64_t` 槽位对齐。类型语义由路由表的 `egw_ctype_t` 字段承载。

### 发布订阅总线 (Pub/Sub Bus)
用 `egw_bus_t` 表示的线程内同步分发总线。生产者以 `(device_id, sig_id, value)` 发布，消费者通过 `egw_bus_subscribe()` 订阅回调。第一版数组式订阅表，O(n) 遍历，回调必须非阻塞。已在 core 中实现（`src/core/egw_bus.c`）。

### 共享上下文 (Context)
用 `egw_context_t` 表示的线程内基础设施容器，持有 `egw_loop_t *` 和 `egw_bus_t *`，不需要的字段为 NULL。通过 `egw_context_init()` 创建（内部创建 loop + bus），`egw_context_destroy()` 销毁。不聚合应用层状态，线程间各自独立。定义在 `src/core/include/egw_context.h`。

### 运行时值持久化 (Runtime Value Persistence)
以 `egw_persist_t` 管理的内存 mmap 文件，保存测点当前值。槽位使用 seqlock（代数 gen + `uint64_t` 裸值），主回路写值时置脏页位（零开销），flush 时扫描脏页按 4KB 页粒度 `pwrite` 落盘。已在 `src/persist/` 中实现。参考 DS-008。

---

## 原有术语（已确认，继续有效）

- **状态机 (Finite State Machine)**：用 `egw_fsm_t` 表示通用状态机引擎。状态通过函数指针表示，返回 `egw_ret_t{target}` 驱动转移。引擎自动投递 `EGW_ENTRY_SIG`/`EGW_EXIT_SIG` 保留信号，状态函数在 entry 中做初始化、在 exit 中做清理。以 `egw_fsm_init` 初始化（自动执行初态 entry），`egw_fsm_dispatch` 派发事件。已在 core 中实现（`src/core/egw_fsm.c`、`src/core/include/egw_fsm.h`），应用层和协议层均可使用。
- **线程池集成**：如果未来出现加密解密等计算密集型任务，通过异步投递机制将任务移出主事件循环线程执行。
