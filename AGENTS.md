# AGENTS.md

## 构建与测试

所有命令前先加载环境：
```bash
source aclass.env.sh
```

| 命令 | 作用 |
|------|------|
| `build` | cmake（Ninja） + 编译。**必须安装 `ninja`** |
| `release` | Release + 安装到 `install/` |
| `clean` / `rebuild` | 清理 / clean + build |
| `run [db_path]` | 运行网关（仅 x86_64），默认 `config.db` |

输出二进制：`build/bin/EdgeGateWay`

```bash
ctest --test-dir build                    # 全部测试
ctest --test-dir build -R test_egw_loop   # 运行单个测试
```

Debug 用 `-Wall -Wextra`，Release 加 `-Werror`。无 lint/类型检查命令。

首次克隆后运行 `submodule-sync` 同步 `third-party/`（cjson、libuv、sqlite、unity）。

`.project.config` 中修改 `ARCH`（`x86_64`/`armv7`）后下次构建自动清 `build/`。

完整测试流程见 `docs/testing.md`。

## 配置体系

- `.project.config`（已提交）— 每行 `KEY=VAL` 作为 `-DKEY=VAL` 传给 CMake
- `.project.local.config`（gitignored）— 交叉编译写 `COMPILE_PATH`/`SYSROOT_PATH`。**先**加载，同名被 `.project.config` 覆盖
- 运行时配置 `config.db`（SQLite），通过 `tools/init_db.py` 初始化
- head 树版本校验：`egw_ptable_head_load()` 返回的 version 必须与 `egw_ptable_open()` 的 DB version 一致

## 设计权威来源

**`CONTEXT.md`（术语表）和 `docs/design.md`（ADR）为准**。代码库已同步设计文档。修改时以文档意图为准，而非以现有实现推断。

### 尚未实现的设计

- Lua 脚本引擎（DS-004）

## 模块依赖与数据流

```
src/
├── app/         入口 main.c → egw_app_run()，libuv 事件驱动
├── core/        核心库：错误码(egw_err)、CRC(egw_crc)、配置(config)、
│                总线(egw_bus)、状态机(egw_fsm)、类型定义(egw_defs)
├── transport/   纯 I/O 工具层：egw_transport（串口/TCP 统一 handle）
├── protocol/    帧定界(egw_protocol) + Modbus RTU/TCP(egw_modbus)
├── ptable/      点表加载（SQLite → 内存连续数组）
└── (persist/    已移除)
```

**链接依赖链**（CMake `target_link_libraries`）：
`app → core, transport, protocol, ptable`
`protocol → core`（不依赖 transport）
`transport → core`（不依赖 protocol）

**运行时数据流**（理解系统的关键）：
```
uv_poll fd 就绪 → egw_transport_read（读原始字节，直入 proto reserve 缓冲区）
  → egw_proto_commit（帧定界 + CRC 校验，零拷贝）
  → frame 就绪 → egw_modbus 解析 → bus_publish(sig_id, value)
  → 回到 idle 等待下一事件
```

**入口执行序列**（`src/app/gateway_app.c:egw_app_run`）：
1. `egw_ptable_head_load(db_path)` — 加载 head 树（纯内存）
2. `egw_ptable_open(db_path, head->version)` — 打开 DB + 校验 version
3. 遍历 head 树 → protocol 节点 → `egw_ptable_discover` → `egw_ptable_register` 三张表
4. `run_modbus_loopback()` — uv_poll 驱动的本地 Modbus 回环演示
5. `egw_ptable_close` + `egw_ptable_head_free` — 清理资源

## 核心约定

### 代码风格
- C11（无 extensions），仅 GCC/Clang。Linux 自动定义 `_GNU_SOURCE`
- `if/else/while/for` **必须**用花括号（MISRA 15.6）
- `.c` 文件用 `/* ── 主题 ── */` 分区；禁止 `//` 注释
- 公开 `.h` 函数写 `/** @brief @param @return */` Doxygen
- 公共 API 前缀 `egw_`；句柄 `egw_{module}_t`（无 `_handle`）
- 新功能独立为 `src/xxx/` 静态库 + `include/` 公开头

### 错误码
- `egw_err_t = int32_t`，`EGW_OK = 0`，负值为错误
- X-macro 表 `egw_err.inc` 单点定义，enum 和 `egw_err_str()` 通过 `#include` 自动生成
- 新增：在 `egw_err.inc` 末尾追加 `EGW_ERROR_CODE(ERR_FOO, -N, "描述")`
- 返回时用 `EGW_RET_CODE(ERR_FOO)` 宏
- 错误码按**错误性质**命名（如 `ERR_OPEN`、`ERR_INVALID_ARG`），不体现模块

### 关键模块角色
- **Transport**：纯同步非阻塞 I/O，不依赖 libuv，无回调、无状态机。handle 式抽象（`egw_transport_handle_t`），fopen 风格 open/close
- **App**：直接使用 libuv（`uv_poll_t`/`uv_timer_t`/`uv_signal_t`），不封装中间层。编排 I/O，不解析协议
- **Protocol**：帧定界 + CRC，零拷贝 reserve/commit 路径。`egw_proto_ctx_create(buf_size, dir)` 区分主站/从站
- **Modbus**：单文件完整实现（`egw_modbus.c`），Client 状态机 + Server 状态机，RTU/TCP 双传输
- **总线值**：`egw_value_t` 为 8 字节无判别式 union（`b/i16/u16/i32/u32/i64/u64/f32/f64/raw`）
- **模块自注册**：`EGW_EXPORT(func, prio)` → `__attribute__((constructor(prio + 101)))`

### 零拷贝路径（io_uring registered-buffer 模式）
```c
/* transport 直接读入 protocol 的 reserve 缓冲区，省掉一次 memcpy */
size_t avail = 0;
uint8_t *wp = egw_proto_reserve(ctx, &avail);     /* 或 egw_modbus_req_reserve / server_reserve */
egw_transport_read(h, wp, &rlen, avail);           /* 直读 proto buf */
egw_proto_commit(ctx, rlen);                       /* 无 memcpy，触发帧定界 */
```

## 线程化扩展约束（ADR-0010）

即使当前单线程，以下必须遵守：

1. 禁止文件级 `static` 运行时数据、`strtok`/`strerror`/`localtime` 等非可重入函数。运行时状态通过参数显式传递
2. 模块 API 优先线程隔离（每线程独立句柄），只读资源可跨线程
3. >10ms 功能设计为可移出主线程
4. 阻塞 I/O 禁止在主事件循环线程执行
5. 跨线程数据传递：所有权转移或 `_Atomic ref_count`

## 学习路径（按阅读顺序）

1. `README.md` — 项目概览
2. `CONTEXT.md` — 术语表，理解领域语言
3. `docs/design.md` — 了解设计决策动机
4. `src/app/gateway_app.c:egw_app_run()` — 入口函数，理解整体骨架
5. `src/transport/include/egw_transport.h` — 传输层统一抽象
6. `src/protocol/egw_protocol.h` — 帧定界 API（reserve/commit 零拷贝）
7. `src/protocol/egw_modbus.h` — Modbus 协议（Client/Server + 点表字段）
8. `src/ptable/include/egw_ptable.h` — 点表加载 API
9. `src/core/include/egw_bus.h` — 总线
10. `src/core/include/egw_fsm.h` — 状态机引擎
11. `tests/test_egw_loop.c` — 从测试理解 libuv 用法

## 重构注意事项

- **Transport 层不依赖 libuv**——切勿在此引入 uv_* 调用或回调
- **Protocol 层不依赖 Transport**——两者均只依赖 core，app 编排
- **错误码枚举**已标记过时或不再使用的条目可用但不应移除已有编号（保持顺序编号不变）
- 修改 `.h` 公开 API 签名时同步更新所有调用点
- `config.db` 为运行时产物，不在 repo 中（用 `python3 tools/init_db.py` 初始化）
- 虚拟串口测试用 `./tools/virtual_serial.sh start`（start/stop/status/restart）

## 参考

- `CONTEXT.md` — 领域术语表
- `docs/design.md` — 设计决策记录（ADR）
- `docs/implementation.md` — 实施记录
- `docs/testing.md` — 测试流程
