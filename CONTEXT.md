# 项目领域模型

## 术语表

### 错误码 (Error Code)
全局统一的模块故障返回值，类型为 `egw_err_t`（`int32_t`）。`EGW_OK = 0` 表示成功，负值表示错误。顺序编号，新增在末尾追加。模块归属通过宏命名前缀区分（如 `EGW_ERR_MQTT_*`、`EGW_ERR_MODBUS_*`），不按值域分段。

### 错误码命名前缀 (Error Code Name Prefix)
宏名称中的模块标识部分，用于在代码中直观区分错误来源。前缀约定：通用错误无模块前缀（`EGW_ERR_*`），模块错误带模块前缀（`EGW_ERR_MQTT_*`、`EGW_ERR_MODBUS_*`）。

### 不透明句柄 (Opaque Handle)
表示模块内部状态的不透明指针类型。命名模式：`egw_{module}_t`（如 `egw_conf_t`），不加 `_handle` 或 `_h` 后缀。生命周期使用领域动词：`load/free`、`connect/disconnect`、`open/close`。与 cJSON 风格一致。

### 代码风格 (Code Style)
遵循 MISRA C:2012 Rule 15.6：所有 `if`/`else`/`while`/`for` 体必须是花括号包裹的复合语句，禁止裸语句。Early return 允许（不强制单出口点），但每个 return 必须有花括号。清理动作独立成行，不与 return 挤在同一行。

### 键路径 (Key Path)
配置查询使用 JSON Pointer（RFC 6901）语法，以 `/` 分隔层级，数组下标直接用数字。如 `/modbus/serial_ports/0/path`。不支持含 `~` 或 `/` 的键名（无需转义）。

### 传输层 (Transport)
负责字节流读写的 I/O 通道，不涉及协议解析。例如串口（UART）、TCP 连接、UDP socket。只关心两件事：从通道收字节打包成 packet 推入队列，将队列中的 packet 字节写入通道。不负责配置校验、重连、生命周期管理。全部异步回调模型，基于 libuv event loop。创建时注册回调（open/data/write/close），open 也是异步。回调指针可为 NULL（表示不关心该事件），但 on_data 为 NULL 会导致数据丢弃。多态通过 vtable（函数指针表）实现。回调存放于基类 egw_transport_t。libuv handle 内嵌在具体变种结构体中。测试替换点暂不实现，将来在 params 中加 fd_override 字段即可。

### 协议层 (Protocol)
负责解析 packet 的语义和帧边界检测。例如 Modbus RTU 帧解析、MQTT 报文编解码。Protocol 层从队列消费 packet，内部完成帧拼接和拆包（Framer），搭载在 Transport 层之上。Framer 维护自己的缓冲区索引，按来源快速定位未完成的半帧。

### 数据包 (Packet)
Transport 和 Protocol 之间传递消息的载体，参考 FFmpeg AVPacket 设计。引用计数 + 对象池，指针传递零拷贝。字段包括：buffer 指针、数据长度、来源标识（整型 ID）、调试追踪序号（递增 uint32_t）、时间戳。本阶段暂不实现，等 Protocol 层时再定。

### 通道 (Channel)
Transport 和 Protocol 之间的双队列连接，包含 rx_queue（Transport → Protocol）和 tx_queue（Protocol → Transport）。对外暴露为一个 egw_channel_t 句柄，两个队列各自独立加锁和唤醒。本阶段暂不实现，等 Protocol 层时再定。

### 守护进程 (Daemon)
负责校验配置、初始化 Transport、断线重连后重新交付 Transport 的管理模块。当前阶段与 main loop 同进程同线程，后期计划剥离为独立 OS 进程。