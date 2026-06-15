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
| `run [-c <path>]` | 运行网关（仅 x86_64），默认配置 `config.json` |

输出二进制：`build/bin/EdgeGateWay`

```bash
ctest --test-dir build                    # 全部测试
ctest --test-dir build -R test_serial     # 运行单个测试
build/bin/test_transport                  # 直接运行测试二进制
```

Debug 用 `-Wall -Wextra`，Release 加 `-Werror`。无 lint/类型检查命令。

首次克隆后运行 `submodule-sync` 同步 `third-party/`（cjson、libuv、unity）。

`.project.config` 中修改 `ARCH`（`x86_64`/`armv7`）后下次构建自动清 `build/`。

## 配置体系

- `.project.config`（已提交）— 每行 `KEY=VAL` 作为 `-DKEY=VAL` 传给 CMake
- `.project.local.config`（gitignored）— 交叉编译写 `COMPILE_PATH`/`SYSROOT_PATH`。**先**加载，同名被 `.project.config` 覆盖
- 运行时配置 `config.json`，路径为 JSON Pointer（RFC 6901），如 `"/modbus/serial_ports/0/path"`
- `egw_conf_t` 句柄**不可跨线程共享**

## 设计权威来源

**`CONTEXT.md`（术语表）和 `docs/design.md`（ADR）为准**。代码库尚未完全同步设计文档（渐进迁移中）。修改时以文档意图为准，而非以现有实现推断。

### 尚未实现的设计

- Lua 脚本引擎（DS-004）
- 点表离线构建工具（DS-003）
- 后台持久化线程（DS-008）

## 模块依赖与数据流

```
src/
├── app/         入口 main.c → egw_app_run()
├── core/        核心库：事件循环(egw_loop)、配置(config)、
│                总线(egw_bus)、共享上下文(egw_context)、
│                错误码(egw_err)、状态机(egw_fsm)
├── transport/   纯 I/O 工具层：egw_serial（open/read/write/flush）
├── protocol/    Modbus RTU 帧解析 FSM
├── ptable/      点表 mmap 加载器
└── persist/     运行时值持久化（脏页位图 + seqlock）
```

**链接依赖链**（CMake `target_link_libraries`）：
`app → core, transport, protocol, ptable, persist`

**运行时数据流**（理解系统的关键）：
```
timer 触发 → 调度下一个端口
  → serial_write/flush（发 Modbus 请求）
  → egw_poll 等待 fd 就绪
  → serial_read（读原始字节）
  → proto_feed（帧定界 + CRC 校验）
  → frame 就绪 → bus_publish(sig_id, value)
  → persist_set（脏页标记）
  → 回到 idle 等待下一 tick
```

**入口执行序列**（`src/app/gateway_app.c:egw_app_run`）：
1. 创建事件循环、总线和 `egw_context_t`
2. 加载 `config.json`（`egw_conf_load`）
3. 打开串口（`egw_serial_open`），每端口创建 `egw_proto_ctx_t`
4. 启动采集 timer（1s） + 注册 SIGINT
5. `egw_loop_run(loop)` → libuv 事件循环驱动一切
6. Ctrl+C → FSM 切到 shutdown → 清理资源

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
- **Transport**：纯同步非阻塞 I/O，不依赖 libuv，无回调、无状态机
- **App**：通过 `egw_fsm_t` 状态机驱动，用 `egw_loop_t`（poll/timer/signal）管理事件，不直接调 `uv_*`
- **Protocol**：状态机驱动，`egw_proto_feed()` 推入字节后同步返回 FRAME_READY / NEED_MORE / FRAME_ERROR
- **总线值**：`egw_value_t` 为 8 字节无判别式 union（`b/i16/u16/i32/u32/i64/u64/f32/f64/raw`）
- **模块自注册**：`EGW_EXPORT(func, prio)` → `__attribute__((constructor(prio + 101)))`

## 线程化扩展约束（ADR-0010）

即使当前单线程，以下必须遵守：

1. 禁止文件级 `static` 运行时数据、`strtok`/`strerror`/`localtime` 等非可重入函数。运行时状态通过 `egw_context_t` 或参数显式传递
2. 模块 API 优先线程隔离（每线程独立句柄），mmap 只读资源可跨线程
3. >10ms 功能设计为可移出主线程
4. 阻塞 I/O 禁止在主事件循环线程执行
5. 跨线程数据传递：所有权转移或 `_Atomic ref_count`

## 学习路径（按阅读顺序）

1. `README.md` — 项目概览
2. `CONTEXT.md` — 术语表，理解领域语言
3. `docs/design.md` — 了解设计决策动机
4. `src/app/gateway_app.c:egw_app_run()` — 入口函数，理解整体骨架
5. `src/core/include/egw_loop.h` — 事件循环封装
6. `src/transport/include/egw_serial.h` — 串口 I/O
7. `src/protocol/egw_protocol.h` — 协议解析 API
8. `src/core/include/egw_bus.h` — 总线
9. `src/core/include/egw_fsm.h` — 状态机引擎（entry/exit 自动编排 + 返回值驱动转移）
10. 各 `*_test.c` — 从测试理解用法

每个模块下可能有 `LEARN.md`（如 `src/app/LEARN.md`），记录了学习笔记。

## 重构注意事项

- **Transport 层不依赖 libuv**——切勿在此引入 uv_* 调用或回调
- **错误码枚举**已在 `egw_err.inc` 标记过时或不再使用的条目可用但不应移除已有编号（保持顺序编号不变）
- 修改 `.h` 公开 API 签名时同步更新所有调用点
- 持久化文件 `gateway_persist.bin` 为运行时产物，不在 repo 中
- `config.json` 中的串口路径、波特率等测试时确保与硬件/仿真环境一致

## 参考

- `CONTEXT.md` — 领域术语表
- `docs/design.md` — 设计决策记录（ADR）
- `docs/implementation.md` — 实施记录
