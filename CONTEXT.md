# 项目领域模型

## 术语表

### 错误码 (Error Code)
全局统一的模块故障返回值，类型为 `egw_err_t`（`int32_t`）。`EGW_OK = 0` 表示成功，负值表示错误。顺序编号，新增在末尾追加。错误码按**错误性质**命名（如 `EGW_ERR_NOTFOUND` 表示"不存在"），不体现模块归属。枚举定义通过 X-macro 表 `egw_err.inc` 自动生成，配套 `egw_err_str()` 函数输出 `"EGW_ERR_INVALID_ARG (-3): invalid argument"` 格式的字符串。

### 错误码命名 (Error Code Naming)
命名描述错误性质，不体现模块来源。模块来源通过函数名追踪。定义在 `egw_err.inc` 中单点维护，`egw_defs.h` 通过 `#include` 展开为 `enum`，`egw_err.c` 展开为 `egw_err_str()` switch 语句。

### 点表数据库 (Point Table Database)
用 SQLite 单文件 `config.db` 存储全部点表配置。启动时通过 `egw_ptable_open()` 打开 + 校验版本，`egw_ptable_register()` 全量 `SELECT *` 加载到内存连续数组后 `close`。运行时不再访问数据库。字段映射通过 `egw_field_t` + `EGW_FIELD` 宏定义，列名与 SQL 列名对应。数据库 schema 由 `tools/init_db.py` 初始化。

### Head 树 (Head Tree)
`egw_head` 表的树形结构，描述网关拓扑。根节点 `HEAD` 作魔数校验，子节点 `thread` → `protocol`/`port`/`sqlite`。通过 `egw_ptable_head_load()` 加载为纯内存 `egw_head_t` 句柄（内部开库 → 解析 → 关库），与 `egw_ptable_open()` 生命周期完全分离。

### Manifest (清单表)
`egw_manifest` 表，记录协议名 → 业务表名的映射。`egw_ptable_discover()` 扫描 manifest + `sqlite_master`，返回匹配的业务表列表。App 遍历 manifest 注册各表。

### 南向点表 (Southbound Point Table)
每个 Modbus 设备的测点协议清单，存储在 `southbound` 表中。描述如何在线缆上解析字节以及本侧的值处理：功能码、寄存器起始地址、数量、南向交互类型（uint16/int32/float 等）、采集周期（毫秒）、是否启用，以及**本侧转换系数**（scale/offset）和**可选的本侧死区**（deadband）。`egw_modbus_master_t` 结构体定义在 `egw_modbus.h`，字段表 `egw_modbus_master_fields()` 供 ptable 注册复用。

转换系数与死区遵循「谁用谁存」：南向自己持有，不集中在路由表。sig_id 的索引与对齐统一归路由表（见 [[路由表]]）。

### 北向点表 (Northbound Point Table)
网关作为 Modbus 从设备的数据模型定义，存储在 `northbound` 表中。描述如何把核心类型值编码到北向协议以及本侧的值处理：功能码、寄存器地址、北向目标类型、转换系数（scale/offset）和死区（deadband）。`egw_modbus_slave_t` 结构体定义在 `egw_modbus.h`，字段表 `egw_modbus_slave_field()` 供 ptable 注册复用。

### 路由表 (Routing Table)
协议无关的信号映射表，存储在 `route` 表中。`egw_route_entry_t` 结构体定义在 `egw_defs.h`（与 `egw_buf_t`/`egw_field_t` 同级），字段表 `egw_ptable_route_fields()` 定义在 `egw_ptable.c`。路由表**不存**转换系数和死区——这些是「谁用谁存」，由南北向各自持有。参考 DS-005。

### 传输层 (Transport)
纯同步非阻塞 I/O 工具层，负责串口/TCP 的 open/read/write/close，不涉及协议解析。**不持有事件循环、不注册回调、不运行状态机**——只提供 handle 级别的字节流读写接口。App 层负责编排：通过 `uv_poll_t` 注册 fd 就绪 → 调用 transport read → 将字节喂入 Protocol → 处理完整帧。当前实现：`egw_transport_serial.c`, `egw_transport_tcp.c`。

### 传输句柄 (Transport Handle)
用 `egw_transport_handle_t *` 表示（opaque struct，定义在私有 `egw_transport_internal.h`）。由 `egw_transport_serial_open()` 或 `egw_transport_tcp_open()` 创建（fopen 风格，失败返回 NULL），通过 `egw_transport_read/write/close` 操作。内部 `fd` 通过 `egw_transport_get_fd()` 只读访问（供 `uv_poll_t` 注册）。`EGW_TRANSPORT_OPEN` 宏通过 `_Generic` 自动分派串口/TCP。

### 协议层 (Protocol)
负责帧定界 + CRC 校验。每个连接独立一个 `egw_proto_ctx_t` 上下文。两种喂入方式：

1. **reserve/commit**（零拷贝，推荐）—— io_uring registered-buffer 模式：`egw_proto_reserve()` 暴露内部缓冲区可写指针，transport 直接 read 入这块内存，`egw_proto_commit()` 触发帧定界 + CRC。省掉一次 memcpy。
2. **feed**（兼容）—— `egw_proto_feed(ctx, data, len)`，数据已在调用方缓冲区时使用。

`egw_proto_ctx_create(buf_size, dir)` 创建上下文，`dir` 区分主站（解析响应）和从站（解析请求），因为同一功能码的请求帧和响应帧长度结构不同。当前实现 Modbus RTU/TCP 帧解析（`src/protocol/egw_protocol.c`）。

### Modbus 协议 (Modbus Protocol)
完整 Modbus RTU/TCP 实现，单文件 `egw_modbus.c`（nanoMODBUS 风格）：

- **PDU 构建/解析**：传输无关，FC01-04 读、FC05/06 写单值、FC0F/10 写多值
- **Client（主站）**：`egw_modbus_req_t` 单次请求上下文，状态机 IDLE→SENDING→WAITING→DONE/ERROR，`done_cb` 回调通知完成
- **Server（从站）**：`egw_modbus_server_t` 长生命周期句柄，喂入请求字节 → 调用 `read_cb`/`write_cb` 用户回调 → 生成响应帧
- **CRC**：调 core 层 `egw_crc_modbus_table()`（查表法）
- **点表字段**：`egw_modbus_master_field()`/`egw_modbus_slave_field()` 返回 `static const egw_field_t[]`，供 ptable 注册复用

### 总线值 (Bus Value)
用 `egw_value_t` 表示的无判别式 8 字节 union（`b/i16/u16/i32/u32/i64/u64/f32/f64/raw`）。类型语义由路由表的 `egw_ctype_t` 字段承载。

### 发布订阅总线 (Pub/Sub Bus)
用 `egw_bus_t` 表示的线程内同步分发总线。生产者以 `(device_id, sig_id, value)` 发布，消费者通过 `egw_bus_subscribe()` 订阅回调。第一版数组式订阅表，O(n) 遍历，回调必须非阻塞。已在 core 中实现（`src/core/egw_bus.c`）。

### 事件循环 (Event Loop)
App 层直接使用 libuv（`uv_loop_t`、`uv_poll_t`、`uv_timer_t`、`uv_signal_t`），不封装中间层。当前单线程，事件循环驱动所有 I/O 和定时器。Transport 和 Protocol 层不依赖 libuv。

### 状态机 (Finite State Machine)
用 `egw_fsm_t` 表示通用状态机引擎。状态通过函数指针表示，返回 `egw_ret_t{target}` 驱动转移。引擎自动投递 `EGW_ENTRY_SIG`/`EGW_EXIT_SIG` 保留信号。已在 core 中实现（`src/core/egw_fsm.c`、`src/core/include/egw_fsm.h`）。

---

## 原有术语（已确认，继续有效）

- **线程池集成**：如果未来出现加密解密等计算密集型任务，通过异步投递机制将任务移出主事件循环线程执行。

---

## 已废弃术语

- **运行时值持久化 (Runtime Value Persistence)**：`egw_persist_t` 已移除，运行时值不持久化，需要时直接 SQLite INSERT。
- **共享上下文 (Shared Context)**：`egw_context_t` 已移除，组件间通过参数显式传参。
- **点表 mmap 二进制**：已替换为 SQLite 存储。
