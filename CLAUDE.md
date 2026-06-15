# CLAUDE.md

本文件为 Claude Code（claude.ai/code）在此仓库中工作时提供指导。

## 构建、运行与测试

所有项目命令都需要先加载环境脚本中的 shell 函数：

```bash
source aclass.env.sh
build                    # 使用 CMake/Ninja 配置并执行 Debug 构建
release                  # 执行 Release 构建并安装到 install/
clean                    # 删除 build/
rebuild                  # clean + build
run                      # 在 x86_64 上运行 build/bin/EdgeGateWay，默认使用 config.json
run -c config.json       # 使用指定配置文件运行
```

测试框架为 Unity，由 `ACLASS_ENABLE_TESTS=ON` 控制：

```bash
ctest --test-dir build            # 运行全部测试
ctest --test-dir build --verbose  # 运行全部测试并输出详细日志
build/bin/test_transport          # 直接运行 transport 仿真测试
build/bin/test_serial             # 直接运行串口测试
```

仓库未记录独立的 lint 或静态分析命令。Release 构建会额外启用 `-Werror`；Debug 构建使用 `-Wall -Wextra`。

## 配置与依赖

- 需要安装 `ninja`（Debian/Ubuntu 可用 `sudo apt install ninja-build`）。
- 首次克隆后运行 `submodule-sync` 同步 `third-party/` 子模块。
- `.project.config` 已提交到仓库，并将每行 `KEY=VAL` 作为 CMake `-DKEY=VAL` 参数传入；`.project.local.config` 被 git 忽略，用于本地覆盖，例如 `COMPILE_PATH` 和 `SYSROOT_PATH`。
- 本地配置先于 `.project.config` 加载，因此 `.project.config` 中的同名变量会覆盖本地值。
- 修改 `.project.config` 中的 `ARCH` 后，下次构建会通过 `.build_arch` 标记自动清理 `build/`。
- 构建产物位于 `build/bin/` 和 `build/lib/`；Release 安装到 `install/`。

## 架构概览

DataGatewayHub 是 Linux 边缘网关，用于通过 RS-485 采集 Modbus RTU 数据、在内部进行数据分发，并向北向系统上报。

```text
Modbus 设备 -> Transport 字节流 -> Protocol 帧 -> App 编排 -> Core 数据模型 / Pub/Sub -> MQTT、Lua、缓存
```

主要层次：

- `src/core/` 构建 `egw_core`：公共定义、错误处理、JSON Pointer 配置，以及规划中的点表和 Pub/Sub 基础能力。
- `src/transport/` 构建 `egw_transport`：基于 libuv 的异步字节 I/O，覆盖串口/TCP 类传输；不包含协议语义。
- `src/protocol/` 构建 `egw_protocol`：协议帧定界、解析与组装，例如 Modbus RTU/TCP 和 MQTT；当前许多能力仍是桩或规划状态。
- `src/script/` 是规划中的 Lua 5.4 协程脚本层；已有文档设计，但尚未完整构建。
- `src/app/` 构建网关应用：加载配置、注册传输连接、持有编排逻辑并驱动事件循环。
- `third-party/` 保存 git 子模块（`cjson`、`libuv`、`unity`）；新增依赖时需要在 `third-party/CMakeLists.txt` 注册。
- `tests/` 保存 Unity 测试，包括 `tests/transport_sim/` 下的测试目标。

预期运行模型是围绕 libuv 的单线程编排：Transport 上抛字节，Protocol 转换为帧，App 查询点表/领域状态，再通过 Core/Pub/Sub 分发给北向上报、Lua 脚本、缓存或南向命令执行。

运行时模型（ADR-0007）：
- 第一版只实现单线程；所有 I/O、协议处理、总线分发和轻量 Lua 协程都在主线程内完成。
- 每个任务线程持有线程内单例 `egw_runtime_t`，通过 `egw_runtime_current()` 访问（不跨线程共享、不传参）。
- `egw_runtime_t` 聚合：`egw_loop_t`（事件循环）、`egw_bus_t`（线程内 Pub/Sub 总线）、`egw_ptable_t`（只读点表视图）、`lua_State`（本线程脚本状态）。
- 未来多线程扩展时，每个线程内部仍是事件驱动 runtime；跨线程消息通过 libuv 的 `uv_async_send` 投递，由目标线程在自己的总线上继续发布。
- 线程内总线发布是同步分发（`egw_bus_publish()` 同步调用所有订阅者回调），订阅者回调必须非阻塞——异步消费者（MQTT、Lua、SQLite）只在回调内提交任务到事件循环，立即返回。

## 领域模型

`CONTEXT.md` 中的术语定义是权威来源。重要概念包括：

- 南向表示网关面向现场设备的 Modbus Master 采集与控制。
- 北向表示面向云平台/管理平台的暴露与上报，包括 MQTT 和 Modbus TCP Slave 行为。
- `sig_id` 是南向点表定义与北向上报/配置之间的桥梁键。
- 点表规划为通过 `mmap` 访问的 `.bin` 文件，内嵌完美哈希索引以支持 O(1) 查询和热更新。
- Pub/Sub 在内部分发 `(device_id, sig_id, value)` 记录；总线值是核心类型的裸值（无类型标识），类型语义由路由表的 `egw_ctype_t` 承载，消费者按 `(device_id, sig_id)` 查路由表解释。**sig_id 是设备内唯一标识**，必须用 `(device_id, sig_id)` 复合键才能唯一标识一个测点。
- 路由表（`routing.bin`）承载三类内容：`(device_id, sig_id)` 索引、双向地址映射（含南向写地址冗余，命令下行时直接取）、核心数据类型约定；不存转换系数和死区（遵循「谁用谁存」，南北向点表各自持有）。
- App 负责编排 Transport、Protocol、Core 和 Script；Transport 与 Protocol 不应直接互相调用。

## 关键实现模式

- 错误码由 `src/core/include/egw_err.inc` 表驱动；新增错误码时在其中追加 `EGW_ERROR_CODE(ERR_FOO, -N, "description")`，不要手工同步 enum 或字符串表。
- 不透明句柄使用 `egw_{module}_t` 命名，不加 `_handle` 或 `_h` 后缀；生命周期动词按模块语义使用 `load/free`、`open/close` 或 `create/destroy`。
- Transport 多态以 `struct egw_transport_header` 作为具体 transport 类型的第一个字段，配合 vtable 函数指针和 `egw_write`、`egw_close` 等 `_Generic` 宏。
- `EGW_EXPORT(func, prio)` 通过 constructor attribute 在 `main()` 前执行；内部优先级会偏移 `+101`，避开编译器保留区间。
- 配置路径使用 RFC 6901 JSON Pointer，例如 `/modbus/serial_ports/0/path`；读取时使用 `egw_conf_get_string()`、`egw_conf_get_int()`、`egw_conf_get_bool()` 和 `egw_conf_array_length()`。
- `egw_conf_t` 不能跨线程共享；每个线程需要独立加载自己的配置句柄。
- 总线值表示（ADR-0009）：`egw_value_t` 是 8 字节无判别式 union（`b/i16/u16/i32/u32/i64/u64/f32/raw`），核心类型枚举通过 `egw_ctype.inc` X-macro 定义；`raw` 成员与 ADR-0008 的 `_Atomic uint64_t` 持久化槽位对齐，seqlock 和业务共用同一份内存。
- 运行时值持久化（ADR-0008）：主回路更新内存当前值表，通过按位或置脏页位；独立持久化线程定期扫描脏页位图，按 4KB 页粒度落盘，使用 seqlock 读取一致快照；主回路零阻塞，写放大可控。

## 代码约定

- C 语言标准为严格 C11，`CMAKE_C_EXTENSIONS OFF`；支持 GCC 和 Clang，Linux 目标会定义 `_GNU_SOURCE`。
- 遵循 MISRA C:2012 Rule 15.6：所有 `if`、`else`、`while`、`for` 的主体都必须使用花括号。允许 early return。
- 公共 API 使用 `egw_` 前缀。
- `.c` 文件使用 `/* ── Section ── */` 形式的分区注释；不要使用 `//` 行注释。
- 公开头文件可使用可选的文件级 Doxygen 块，并且导出函数需要按需写 `@brief`、`@param`、`@return` 注释。
- 内部头文件只使用分区注释，不写 Doxygen 风格 API 文档。
- 新功能通常应拆成 `src/<module>/` 下的独立静态库，公开头文件放在 `include/`，私有实现文件留在模块目录内。
- 优先复用现有扩展机制（`egw_err_t`、`EGW_EXPORT`、transport vtable、Pub/Sub/Channel 概念），避免另起平行体系。

线程化扩展约束（ADR-0010）：
- 禁止使用线程不安全的全局可变状态（文件级 `static` 变量持有运行时数据、`strtok` 等非可重入函数）；运行时状态归属 `egw_runtime_t` 或模块句柄。
- 模块 API 优先选择线程隔离模型（每线程独立句柄，不跨线程共享），避免提前引入锁。
- 预期执行时间 >10ms 的功能（Lua 重计算脚本、SQLite 批量查询、加密/压缩）必须设计为可移到独立线程执行：封装为纯函数，通过 `uv_async_send` 跨线程传递输入/输出。
- 阻塞 I/O（`pwrite`、`fdatasync`、`sqlite3_step`）不能在主事件循环线程执行，必须隔离到独立线程。
- 跨线程传递数据时，内存所有权必须清晰：所有权转移或引用计数（`_Atomic int ref_count`）。

## 文档参考

- `AGENTS.md` 包含更完整的命令与约定参考。
- `CONTEXT.md` 定义领域模型与统一语言。
- `docs/adr/` 记录架构决策，包括错误码、点表、Lua 脚本、Pub/Sub、单线程事件循环归属等。
- `README.md` 提供项目简介、配置文件、构建命令和目录结构。