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

**决定**：点表以 `.bin` 二进制文件存储，运行时通过 `mmap` 只读访问。固定小端字节序、固定宽度类型、`__attribute__((packed))`。三类文件（南向、北向、路由）共享相同文件头，包含 magic、version、build_id、checksum、字节序标记。同源构建者 build_id 必须一致。转换系数和死区遵循「谁用谁存」，由南北向点表各自持有，路由表不存。

**状态**：已实施（格式定义 + mmap 加载器）。离线构建工具未实现（外部 Python 脚本）。

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

**决定**：Core 层封装 libuv 为 `egw_loop_t`，提供 poll（`egw_poll_t`）、timer（`egw_timer_t`）、signal（`egw_signal_t`）API。Transport、Protocol、App 不直接调用 libuv。原定的 `egw_io_handle_t` 以 `egw_poll_t` 替代。

**状态**：已实施。

---

## DS-007：线程内运行时模型

**决定**：第一版单线程。每个任务线程持有线程内单例 `egw_runtime_t`（聚合 `egw_loop_t`、`egw_bus_t`、`egw_ptable_t`、`lua_State`），通过 `egw_runtime_current()` 访问。多线程扩展时各线程内部仍为事件驱动 runtime，跨线程通过 `uv_async_send` 投递。

**实施调整**：原协程模型替换为 `egw_fsm_t` 状态机。后 `egw_runtime_t` 被移除，替换为 `egw_context_t`（仅持 loop + bus，无文件级静态指针）。

**状态**：已实施（`egw_context_t` 取代 `egw_runtime_t` 聚合 loop + bus，显式传参不依赖全局指针）。

---

## DS-008：运行时值持久化

**决定**：主回路内存为真相，独立持久化线程落盘。脏页位图跟踪变更（4KB 页粒度，1 bit/页），seqlock 读取一致快照（3 次重试 + 50µs 睡眠）。`last_flush_unix_ms` 文件头记录最近落盘时间。已实现协议 `_Atomic uint64_t` 槽位 + 代数 gen。失败保留脏位下周期重试。

**状态**：已实施（`egw_persist_t`，seqlock 槽位 + 脏页位图 + mmap 文件，flush 同步落盘）。后台持久化线程未实现。

---

## DS-009：总线值表示

**决定**：`egw_value_t` 为 8 字节无判别式 union，成员：`b/i16/u16/i32/u32/i64/u64/f32/f64/raw`。`raw` 与持久化 `_Atomic uint64_t` 槽位对齐。核心类型枚举通过 `egw_ctype.inc` X-macro 定义。

**状态**：已实施（`egw_value_t` union + `egw_ctype.inc` 枚举）。

---

## DS-010：线程化扩展约束

**决定**：
1. 禁止线程不安全的全局可变状态（`static` 运行时数据、`strtok`/`strerror`/`localtime`）
2. 模块 API 优先线程隔离模型（每线程独立句柄）
3. >10ms 功能设计为可移出主线程（纯函数 + 异步 API）
4. 阻塞 I/O 禁止在主事件循环线程执行
5. 跨线程传递数据所有权转移或 `_Atomic int ref_count`

线程分类：
- 主线程：持有完整 `egw_runtime_t`，处理 I/O、事件循环、数据分发
- 单一功能线程：只有专属资源 + `uv_async_t` 通信

**状态**：设计约束，持续遵守。

---

## DS-011：FSM 引擎 entry/exit + 返回值驱动转移

**决定**：`egw_fsm_t` 引擎改为类 QP/C 的返回值驱动模式：

- 框架保留 4 个信号：`EGW_ENTRY_SIG`、`EGW_EXIT_SIG`、`EGW_INIT_SIG`、`EGW_USER_SIG`
- 状态函数签名改为返回 `egw_ret_t { target }`，`target == NULL` 表示不转移，非 NULL 表示转移到目标状态
- `egw_fsm_dispatch` 在检测到转移时自动执行：`exit(source) → current = target → entry(target)`
- `egw_fsm_init` 自动执行初始状态的 entry 动作
- 返回值采用结构体而非 QP 的隐藏 temp 字段，减少间接层；不增加 ABI 开销

**动机**：
- 消除了手工调 `st_shutdown(fsm_ptr, NULL)` 这类脆弱模式
- 状态函数的 entry/exit 逻辑由引擎自动编排，不会遗漏清理
- `app_phase_t` 隐式子 FSM 未来可收归引擎管理

**备选方案**：
- QP/C 的 `uint_fast8_t` 返回值 + `Q_TRAN` 宏写隐藏字段 —— 副作用不够透明，HSM 能力当前用不上

**状态**：已实施（`src/core/egw_fsm.c`、`src/core/include/egw_fsm.h`）。

---

## DS-012：移除 egw_runtime_t，替换为 egw_context_t

**决定**：将 `egw_runtime_t`（内含文件级静态指针 `g_runtime` + `egw_runtime_current()` 全局访问器）替换为 `egw_context_t{ loop, bus }`。

- `egw_context_init()` 内部创建 loop + bus，`egw_context_destroy()` 销毁。定义在 `src/core/include/egw_context.h` + `src/core/egw_context.c`
- 无全局静态状态，context 实例在 `egw_app_run` 栈上声明，通过指针显式传参
- 应用层不再有聚合一切的 `app_ctx_t`；每个回调通过 `data` 指针仅拿到自己需要的部分（`port_ctx_t`、`timer_share_t`、`fsm_data_t`）
- FSM 仅负责 running → shutdown 转移；清理在 `egw_loop_run` 返回后统一执行
- `port_ctx_t` 持有 `egw_context_t *` 回指，供 reopen 场景取 loop

**动机**：
- 消除文件级静态可变状态（违反 ADR-0010）
- 组件解耦：不再有一个中心结构体被所有函数引用
- 为未来多线程做准备：每个线程独立声明自己的 context，不共享

**状态**：已实施（`gateway_app.c` + `egw_context.h`）。
