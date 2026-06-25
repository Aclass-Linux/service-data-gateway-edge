# 实施记录

> 早期记录（2026-06-15 ~ 06-17）保留为历史参考。后续模块已大幅重构，详见末尾最新记录。

## 2026-06-15：Core 事件循环 + App FSM + Protocol FSM

### egw_loop_t 扩展（ADR-0006）

原有 `egw_loop_t`（create/run/stop/destroy）扩展为完整的 libuv 封装：

- `egw_poll_t`：fd 就绪通知（替代 `uv_poll_t`）
- `egw_timer_t`：定时器（替代 `uv_timer_t`）
- `egw_signal_t`：信号处理（替代 `uv_signal_t`）

结构体公开在 `egw_loop.h` 中，调用者可嵌入。API 通过回调函数指针注册，内部用 libuv wrapper 转发。

`egw_fsm.h` 中 `egw_signal_t`（事件信号类型）重命名为 `egw_sig_t` 避免冲突。

修复：`egw_loop_run()` 在 `uv_stop()` 后误报 `EGW_ERR_LOOP_RUN` → 改为检查 `should_stop` 标志。（已被后续简化移除：`UV_RUN_DEFAULT` 返回值 >0 必定来自 uv_stop，`should_stop` 冗余。）

> **后续变更**：`egw_loop_t` 封装已移除（DS-006 废弃）。App 层直接使用 libuv。

### Protocol FSM：Modbus RTU 帧解析

`egw_proto_ctx_t` 每端口独立上下文。`egw_proto_feed(ctx, data, len)` 推入字节，内部基于功能码判定期望帧长度 + CRC-16 校验，同步返回：

| 返回值 | 含义 |
|--------|------|
| `EGW_PROTO_FRAME_READY` | 完整帧就绪，`egw_proto_get_frame()` 获取 |
| `EGW_PROTO_NEED_MORE` | 等待更多字节 |
| `EGW_PROTO_FRAME_ERROR` | 帧异常，内部已自动重置 |

> **后续变更**：增加 reserve/commit 零拷贝路径（DS-015）和解析方向参数（DS-016）。

### App FSM：状态机驱动的采集调度

`gateway_app.c` 以 `egw_fsm_t` 驱动，两状态：`st_running` / `st_shutdown`。采集通过定时器调度 → 发 Modbus 请求 → poll 等响应 → Protocol 解析 → 总线发布。错误自动 reopen。

> **后续变更**：`egw_context_t` 已移除（DS-007/DS-012 废弃），App 直接使用 libuv + 显式传参。

### 新增文件（早期，部分已移除）

| 文件 | 用途 | 现状 |
|------|------|------|
| `src/core/include/egw_ctype.inc` | 9 种核心类型 X-macro | 保留 |
| `src/core/include/egw_ptable.h` | 点表二进制格式定义 | **已移除**（DS-013） |
| `src/core/include/egw_bus.h` | Pub/Sub 总线 API | 保留 |
| `src/core/egw_bus.c` | 总线实现（数组订阅表） | 保留 |
| `src/core/include/egw_runtime.h` | 运行时单例 API | **已移除**（DS-007） |
| `src/core/egw_runtime.c` | 运行时实现 | **已移除**（DS-007） |
| `src/ptable/` | 点表 mmap 加载器 | **已重写**（DS-013，SQLite） |
| `src/persist/` | 运行时值持久化 | **已移除**（DS-008） |

## 2026-06-15：FSM 引擎重写 — entry/exit + 返回值驱动转移

`egw_fsm_t` 从纯函数指针覆盖升级为类 QP/C 的返回值驱动模式：

- 新增 `egw_ret_t { target }` 返回值，状态函数返回 `EGW_RETURN(state)` 或 `EGW_RET_HANDLED`
- `egw_fsm_dispatch` 自动做 `exit(source) → 切指针 → entry(target)`
- `egw_fsm_init` 自动执行初态 entry
- 框架保留信号 `EGW_ENTRY_SIG` / `EGW_EXIT_SIG` / `EGW_INIT_SIG` / `EGW_USER_SIG`，用户信号从 `EGW_USER_SIG` 开始

`gateway_app.c` 随之适配：`st_running` / `st_shutdown` 改为返回 `egw_ret_t`；`st_shutdown` 的清理逻辑收归 `EGW_ENTRY_SIG`；删除手工 `st_shutdown(fsm_ptr, NULL)`。

### 设计决策

- 协程方案放弃，用 `egw_fsm_t` 状态机替代
- Transport 层定为纯 I/O 工具层，不持调度器/回调/事件循环
- `egw_loop_t` 提供 poll/timer/signal 最小封装
- 配置查询路径修正为 `"/modbus/serial_ports/N/path"`
- `egw_value_t` 8 字节无判别式 union + seqlock 持久化槽位

## 2026-06-17：协议校验在回调中执行的性能模型

**背景**：确认在 uv_poll 回调中直接执行协议合法性校验（含 CRC-16）是否会造成事件循环阻塞。

**分析方法**：从 CPU 指令级逐层拆解，以 Modbus RTU（极限 256 字节，查表 CRC-16）为靶子测算。

### 时间预算

| 级别 | 单次回调上限 | 校验耗时占比 |
|------|-------------|-------------|
| 及格线 | 1,000 μs | 0.1% |
| 优秀线 | 100 μs | 1% |

### 操作拆解

| 操作 | 指令数 | 耗时 | 说明 |
|------|--------|------|------|
| 帧长度校验 | 1 条比较 | ~1 ns | 仅比较两个整数 |
| 功能码合法性 | 2~3 条指令 | ~10 ns | 位图查表或 switch |
| CRC-16（查表法，254 字节） | 254 × (读+异或+查表) | ~1,000 ns (1 μs) | 表 512B 常驻 L1 Cache |
| **合计** | | **~1 μs** | |

### 结论

在主流嵌入式 Linux 网关（Cortex-A7 @ 1GHz）上，完整协议校验仅耗时约 **1 微秒**，占优秀线 100 μs 预算的 **1%**，完全不构成阻塞风险。即便串口 115200bps 满跑（~45 帧/秒），每秒校验总开销也仅 45 μs。

### 验证的三条纪律（坑与对策）

已在代码中落实，无违规：

| 坑 | 后果 | 本工程做法 |
|----|------|-----------|
| 校验失败时 `printf`/`write` | 系统调用，飙升至几十 ms | `egw_proto_feed` 返回值后静默处理，不输出 |
| CRC 表 Cache Miss | 256 次读 DDR（~50ns/次）→ 25 μs | 512B 查表法，结构紧凑，L1 Cache 常驻 |
| 回调内加锁 | 锁竞争导致挂起 | 回调层无锁，不操作共享状态 |

## 2026-06-17：点表格式从 .bin/mmpp 迁移到 SQLite

### 动机

放弃自研 `.bin` 二进制格式 + `mmap` 零拷贝方案，迁移到 SQLite 做配置持久化：

- 跨架构结构体对齐（`__attribute__((packed))`）是静默 bug 来源，SQLite 消除了序列化/反序列化隐患
- 运维阶段字段变更用 SQL `ALTER TABLE` 平滑迁移
- 启动时全量 `SELECT` 加载到内存有序数组，运行时 `bsearch()` O(log n) 查询
- 运行时值走纯内存结构（不持久化），persist 模块移除

### 已实施

| 操作 | 文件 |
|------|------|
| 删除 | `src/core/include/egw_ptable.h`（二进制格式定义） |
| 删除 | `src/ptable/` 旧加载器（`egw_ptable_loader.c/h` + `CMakeLists.txt`） |
| 删除 | `src/persist/` 模块（mmap 持久化，功能由 SQLite 替代） |
| 清理 | `third-party/sqlite/` 子模块 → 仅保留 amalgamation（`sqlite3.c` + `sqlite3.h` + `sqlite3ext.h`） |
| 新增 | `third-party/sqlite/CMakeLists.txt`：静态库，`SQLITE_THREADSAFE=0` |
| 新增 | `src/ptable/egw_ptable.c`：SQLite 加载三表 → 内存有序数组 → `bsearch()` 查询 |
| 新增 | `src/ptable/include/egw_ptable.h`：`egw_sb_point_t` / `egw_nb_point_t` / `egw_route_entry_t` |
| 修改 | `src/app/gateway_app.c`：移除 persist 引用（include + 字段 + create/destroy） |

### 数据模型

启动流程：
```
egw_ptable_open("config.db")
  → sqlite3_open_v2()
  → CREATE TABLE IF NOT EXISTS (southbound / northbound / route)
  → SELECT * → 内存有序数组 → qsort(device_id, sig_id)
  → sqlite3_close()
```

运行时查询：
```
egw_ptable_sb_lookup(device_id, sig_id)
  → bsearch() 二分查找，O(log n)
```

### 数据库 Schema

三张表，均为单文件 `config.db`：

```sql
CREATE TABLE southbound (
    device_id, sig_id, func_code, reg_addr, reg_count,
    ctype, poll_interval_ms, flags, scale, offset, deadband,
    PRIMARY KEY (device_id, sig_id)
);

CREATE TABLE northbound (
    device_id, sig_id, func_code, reg_addr,
    ctype, flags, scale, offset, deadband,
    PRIMARY KEY (device_id, sig_id)
);

CREATE TABLE route (
    device_id, sig_id, ctype,
    PRIMARY KEY (device_id, sig_id)
);
```

> **后续变更**：schema 改为 `egw_head`（树形）+ `egw_manifest` + 三张业务表，由 `tools/init_db.py` 初始化。`egw_ptable_register` 改为批量返回 `egw_buf_t`，不再回调。详见 2026-06-25 记录。

---

## 2026-06-25：Transport + Protocol + Modbus 全面重构

### Transport 层：统一 handle 抽象（DS-014）

从句柄式 `egw_serial_t` 改为 handle 式统一抽象：

- `egw_transport_handle_t`（opaque struct，定义在私有 `egw_transport_internal.h`）
- `egw_transport_serial_open/egw_transport_tcp_open`：fopen 风格，返回 handle 或 NULL
- `egw_transport_common.c`：read/write/close/get_fd 包装（调函数指针）
- `EGW_TRANSPORT_OPEN` 宏通过 `_Generic` 自动分派串口/TCP
- `int fd` 通过 `egw_transport_get_fd()` 只读获取（供 `uv_poll_t` 注册）
- 移除 `egw_serial.h` / `egw_serial_params.h` / `egw_serial` 旧 API

### Protocol 层：零拷贝 + 解析方向（DS-015, DS-016）

io_uring registered-buffer 模式，消除 transport→protocol 的 memcpy：

- `egw_proto_reserve(ctx, &avail)` → 返回 `ctx->buf + ctx->len` 可写指针
- `egw_proto_commit(ctx, n)` → 更新 len，跑定界 + CRC（无 memcpy）
- `egw_proto_feed` 保留作兼容路径（= reserve + memcpy + commit）
- `egw_proto_ctx_create(buf_size, dir)`：`dir` 区分主站（解析响应）和从站（解析请求）
- 删除本地 `crc16`，改用 core 的 `egw_crc_modbus_table()`
- 修复 VLA（`payload[6+byte_count]` → `payload[EGW_MODBUS_MAX_PDU]`）
- 修复 `parse_read_pdu` 异常码冲突（`-(exc + 100)`，即 -101~-111）

### Modbus 协议：单文件完整实现（DS-017）

`egw_modbus.c`（~900 行）包含：
- CRC（调 core）、帧封装/解封装（RTU + TCP）
- PDU 构建/解析（FC01-04 读、FC05/06 写单值、FC0F/10 写多值）
- Client 状态机（`egw_modbus_req_t`，IDLE→SENDING→WAITING→DONE/ERROR）
- Server 状态机（`egw_modbus_server_t`，feed → parse → callback → response）
- req/server 均提供 reserve/commit 零拷贝路径

### 点表字段表归属（DS-018）

- `egw_modbus_master_fields()`/`egw_modbus_slave_fields()` → protocol 层
- `egw_ptable_route_fields()` → ptable 层
- `egw_route_entry_t` → `egw_defs.h`（协议无关）

### App 层：uv_poll 驱动的本地回环

`gateway_app.c` 实现完整 Modbus RTU 本地回环：

- `uv_poll_t` 监听 server fd → reserve + read + commit → 生成响应 → write
- 切到 `uv_poll_t` 监听 client fd → reserve + read + commit → `req_process` → `done_cb`
- `uv_timer_t` 2 秒超时保底
- `loopback_ctx_t` 聚合所有状态，回调通过 `p->data` 拿回
- 全程零拷贝路径

### 工具脚本

- `tools/virtual_serial.sh`：改为 start/stop/status/restart 管理模式，后台运行 + PID 文件
- `tools/init_db.py`：创建 `egw_head`（树形）+ `egw_manifest` + 三张业务表 + 测试数据
