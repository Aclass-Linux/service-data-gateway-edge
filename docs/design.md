# 设计决策记录

## DS-001：错误码方案

**决定**：`egw_err_t` = `int32_t`。X-macro 表 `egw_err.inc` 单点定义，`egw_defs.h` 展开为 enum，`egw_err.c` 展开为字符串函数。错误码按性质命名（`ERR_OPEN`, `ERR_INVALID_ARG`），不体现模块归属。顺序编号，新增在末尾追加。不与 POSIX errno 混用。

**状态**：已实施。

---

## DS-002：配置键路径

**决定**：JSON Pointer（RFC 6901）语法，如 `/modbus/serial_ports/0/path`。依赖 cJSON_Utils 标准实现，删除自定义解析器。键名不使用 `~` 和 `/`，无需转义。

**状态**：已实施。

---

## DS-003：点表 mmap 二进制

**决定**：点表以 `.bin` 二进制文件存储，运行时通过 `mmap` 只读访问。固定小端字节序、固定宽度类型、`__attribute__((packed))`。

**状态**：**已废弃**。被 DS-013（SQLite 点表存储）替代。packed struct 跨架构对齐是静默 bug 来源，SQLite `ALTER TABLE` 平滑迁移字段，启动时全量加载到内存后 close。

---

## DS-004：Lua 嵌入式脚本

**决定**：使用 Lua 5.4。API 为同步协程风格：`modbus:read(1, 30001)` 底层 `lua_yield()`，响应后 `lua_resume()`。重计算脚本未来调度到独立线程。

**状态**：未实现。

---

## DS-005：内部 Pub/Sub 总线

**决定**：线程内发布订阅总线。南向生产者以 `(device_id, sig_id, value)` 发布，北向消费者订阅。第一版单线程同步分发，订阅者回调必须非阻塞。`egw_value_t` 为 8 字节无判别式 union（类型语义由路由表承载）。路由表承载 `(device_id, sig_id)` 索引、双向地址映射、核心数据类型。线程内总线不引入锁，跨线程通过 `uv_async_send` 投递。

**状态**：已实施（`egw_bus_t`，数组式订阅表，同步分发）。

---

## DS-006：Core 层持有事件循环

**决定**：Core 层封装 libuv 为 `egw_loop_t`，提供 poll/timer/signal API。Transport、Protocol、App 不直接调用 libuv。

**状态**：**已废弃**。`egw_loop_t` 封装已移除。App 层直接使用 libuv（`uv_poll_t`/`uv_timer_t`/`uv_signal_t`），无中间封装层。Transport 和 Protocol 层仍不依赖 libuv。

---

## DS-007：线程内运行时模型

**决定**：每个任务线程持有线程内单例 `egw_runtime_t`（聚合 loop + bus + ptable），通过 `egw_runtime_current()` 访问。后替换为 `egw_context_t`（仅持 loop + bus，无文件级静态指针）。

**状态**：**已废弃**。`egw_runtime_t` 和 `egw_context_t` 均已移除。组件间通过参数显式传参，不依赖全局指针或中心容器。

---

## DS-008：运行时值持久化

**决定**：主回路内存为真相，独立持久化线程落盘。脏页位图跟踪变更（4KB 页粒度），seqlock 读取一致快照。

**状态**：**已废弃**。`egw_persist_t` 已移除，运行时值不持久化。需要持久化时直接 SQLite INSERT。

---

## DS-009：总线值表示

**决定**：`egw_value_t` 为 8 字节无判别式 union，成员：`b/i16/u16/i32/u32/i64/u64/f32/f64/raw`。核心类型枚举通过 `egw_ctype.inc` X-macro 定义。

**状态**：已实施（`egw_value_t` union + `egw_ctype.inc` 枚举）。

---

## DS-010：线程化扩展约束

**决定**：
1. 禁止线程不安全的全局可变状态（`static` 运行时数据、`strtok`/`strerror`/`localtime`）
2. 模块 API 优先线程隔离模型（每线程独立句柄）
3. >10ms 功能设计为可移出主线程（纯函数 + 异步 API）
4. 阻塞 I/O 禁止在主事件循环线程执行
5. 跨线程传递数据所有权转移或 `_Atomic int ref_count`

**状态**：设计约束，持续遵守。

---

## DS-011：FSM 引擎 entry/exit + 返回值驱动转移

**决定**：`egw_fsm_t` 引擎改为类 QP/C 的返回值驱动模式：框架保留 4 个信号（`EGW_ENTRY_SIG`/`EGW_EXIT_SIG`/`EGW_INIT_SIG`/`EGW_USER_SIG`），状态函数返回 `egw_ret_t { target }` 驱动转移，`egw_fsm_dispatch` 自动执行 `exit(source) → entry(target)`。

**状态**：已实施（`src/core/egw_fsm.c`、`src/core/include/egw_fsm.h`）。

---

## DS-012：移除 egw_runtime_t，替换为 egw_context_t

**决定**：将 `egw_runtime_t` 替换为 `egw_context_t{ loop, bus }`，无全局静态状态，context 实例在 `egw_app_run` 栈上声明。

**状态**：**已废弃**。`egw_context_t` 后续也被移除。组件间完全通过参数显式传参。

---

## DS-013：点表存储从 .bin/mmap 迁移到 SQLite

**决定**：放弃自研 `.bin` 二进制格式 + `mmap` 零拷贝方案，迁移到 SQLite 单文件 `config.db`。

**动机**：
- packed struct 跨架构对齐（`__attribute__((packed))`）是静默 bug 来源
- 运维阶段字段变更用 SQL `ALTER TABLE` 平滑迁移
- 启动时全量 `SELECT` 加载到内存连续数组后 `close`，运行时纯内存操作

**实施**：
- `egw_ptable_open(path, head_version)`：打开 DB + 校验 version
- `egw_ptable_register(pt, table, fields, row_size)`：`COUNT(*)` → `calloc` → `SELECT *` → 首行列名解析 → 逐行 `read_column` 批量填充 → 返回 `egw_buf_t {data, len}`
- `egw_field_t` + `EGW_FIELD` 宏：字段映射（DB 列名 → C struct 偏移/类型）
- `egw_head` 表改为树形结构（`id`/`parent_id`/`type`/`desc`）
- `egw_manifest` 表记录协议名 → 业务表名映射

**状态**：已实施。替代 DS-003。

---

## DS-014：Transport 层统一 handle 抽象

**决定**：Transport 层从句柄式 `egw_serial_t` 改为 handle 式统一抽象，串口/TCP 共用同一 `egw_transport_handle_t`（opaque struct）。

**设计**：
- `egw_transport.h`：单头文件入口，含 serial params + tcp params + `EGW_TRANSPORT_READ/WRITE/CLOSE/OPEN/GET_FD` 宏
- `egw_transport_internal.h`：私有 struct 定义（`fd` + `read`/`write`/`close` 函数指针），不对外暴露
- `egw_transport_serial.c` / `egw_transport_tcp.c`：各自实现 open/read/write/close
- `egw_transport_common.c`：read/write/close/getfd 包装
- `EGW_TRANSPORT_OPEN(params)` 通过 `_Generic` 自动分派串口/TCP
- open 返回 handle（fopen 风格），失败返回 NULL
- `int fd` 通过 `egw_transport_get_fd()` 只读获取（供 `uv_poll_t` 注册）

**约束**：
- Transport 不持有事件循环、不注册回调、不运行状态机
- Transport 不依赖 Protocol 头；Protocol 不依赖 Transport 头
- App 编排 I/O，Protocol 纯解析（nginx 风格）

**状态**：已实施。

---

## DS-015：Protocol 层零拷贝喂入（io_uring registered-buffer 模式）

**决定**：Protocol 层提供 reserve/commit 零拷贝路径，消除 transport→protocol 的一次 memcpy。

**设计**：
- `egw_proto_reserve(ctx, &avail)` → 返回 `ctx->buf + ctx->len` 可写指针
- `egw_proto_commit(ctx, n)` → 更新 len，跑定界 + CRC（无 memcpy）
- `egw_proto_feed` 保留作兼容路径（= reserve + memcpy + commit）
- Modbus req/server 同样提供 `reserve/commit`，帧就绪后调 `handle_frame`

**类比**：
- io_uring registered buffers：消费方预发布缓冲区，生产方直接写入
- 方案 A 在 transport↔protocol 边界消除开销，io_uring 在 kernel↔user 边界消除开销，两者可叠加

**状态**：已实施。

---

## DS-016：Protocol 层解析方向参数

**决定**：`egw_proto_ctx_create(buf_size, dir)` 增加 `egw_proto_dir_t` 参数，区分主站（解析响应）和从站（解析请求）。

**动机**：Modbus 请求帧和响应帧对同一功能码有不同的长度结构。例如 FC=03 请求固定 8 字节，响应为 `3 + byte_count + 2` 字节。protocol 层需要知道方向才能正确计算期望帧长度。

**状态**：已实施。

---

## DS-017：Modbus 协议单文件实现

**决定**：完整 Modbus RTU/TCP 实现放在单个 `egw_modbus.c` 文件中（nanoMODBUS 风格），不拆分为多个文件。

**内容**：
- CRC（调 core 的 `egw_crc_modbus_table`）
- 帧封装/解封装（RTU + TCP）
- PDU 构建/解析（传输无关）
- Client 状态机（IDLE→SENDING→WAITING→DONE/ERROR）
- Server 状态机（feed → parse → callback → response）
- 点表字段表（`egw_modbus_master_fields`/`egw_modbus_slave_fields`）

**状态**：已实施。

---

## DS-018：点表字段表归属

**决定**：
- Modbus 特有的 `master_fields`/`slave_fields` 定义在 protocol 层（`egw_modbus.c`），通过 `egw_modbus_master_fields()`/`egw_modbus_slave_fields()` 访问
- 协议无关的 `route_fields` 定义在 ptable 层（`egw_ptable.c`），通过 `egw_ptable_route_fields()` 访问
- `egw_route_entry_t` 定义在 `egw_defs.h`（协议无关）

**动机**：路由表是 ptable 基础设施层的概念，不应让 ptable 反向依赖 protocol 层。

**状态**：已实施。
