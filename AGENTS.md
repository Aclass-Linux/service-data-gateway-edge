# AGENTS.md

## 构建与测试

所有命令必须先加载环境：

```bash
source aclass.env.sh
```

- `build` — cmake 配置（Ninja 生成器）+ 编译。**必须安装 `ninja`**。
- `release` — Release 编译 + 安装到 `install/`。
- `clean` / `rebuild`
- `run` — 运行网关（仅 x86_64）。`-c <path>` 指定配置文件，默认 `config.json`。
- 修改 `.project.config` 中 `ARCH` 后下次构建自动清除 `build/`（通过 `.build_arch` 标记检测）。

测试框架 Unity，`ACLASS_ENABLE_TESTS=ON` 控制：

```bash
ctest --test-dir build               # 全部测试
ctest --test-dir build --verbose
build/bin/test_transport             # transport 仿真测试
build/bin/test_serial                # 串口测试
build/bin/test_egw_loop              # 事件循环测试
```

无 lint/类型检查命令。Release 构建额外启用 `-Werror`；Debug 使用 `-Wall -Wextra`。

## 配置体系

- `.project.config`（已提交）— 每行 `KEY=VAL` 自动作为 `-DKEY=VAL` 传给 CMake。
- `.project.local.config`（gitignored）— 交叉编译时写 `COMPILE_PATH`/`SYSROOT_PATH`。**先**加载，同名变量会被 `.project.config` 覆盖。
- `ARCH`（`x86_64`/`armv7`）决定工具链文件 `cmake/toolchain-{ARCH}.cmake`。
- `ACLASS_PROJECT_NAME=EdgeGateWay`，输出二进制 `build/bin/EdgeGateWay`。
- 首次克隆后：`submodule-sync` 同步 `third-party/` 子模块（cjson、libuv、unity）。

## 设计权威来源

**CONTEXT.md 和 docs/design.md 是架构设计的权威来源**。代码库尚未完全同步到设计文档描述的目标架构（当前处于渐进迁移中）。修改代码时以设计文档为准，而非以现有实现推断设计意图。

当前代码与设计的关键差异见"模块化"节中的"尚未实现的设计"。

## 项目结构

```
src/app/         → 可执行入口 (main.c → egw_app_run)
                   App 用 egw_fsm_t 状态机 + egw_loop_t 封装驱动，不直接调 uv_*
src/core/        → egw_core 静态库：错误码 (egw_err.c)、事件循环 (egw_loop.c)、
                   配置 (config.c)、总线 (egw_bus.c)、运行时 (egw_runtime.c)
  include/       → egw_defs.h, egw_err.inc, egw_ctype.inc, egw_loop.h,
                   config.h, egw_fsm.h, egw_ptable.h, egw_bus.h, egw_runtime.h
src/transport/   → egw_transport 静态库：纯 I/O 工具层 (egw_serial.c)
  include/       → egw_serial.h, egw_serial_params.h
src/protocol/    → egw_protocol 静态库：Modbus RTU 帧解析 FSM (egw_protocol.c)
src/ptable/      → egw_ptable 静态库：点表 mmap 加载器
src/persist/     → egw_persist 静态库：运行时值持久化
third-party/     → git 子模块 (cjson, libuv, unity)
tests/           → transport_sim/ 下 test_transport, test_serial；根级 test_egw_loop
docs/design.md   → 设计决策记录

## 核心约定

### 语言与编译
- 严格 **C11**（`CMAKE_C_EXTENSIONS OFF`），仅 GCC/Clang。Linux 自动定义 `_GNU_SOURCE`。

### 错误码
- `egw_err_t` = `int32_t`。`EGW_OK = 0`，负值为错误。
- 通过 **X-macro** 表 `src/core/include/egw_err.inc` 单点定义。新增步骤：
  1. 在 `egw_err.inc` 末尾追加 `EGW_ERROR_CODE(ERR_FOO, -N, "描述")`
  2. enum 和 `egw_err_str()` 通过 `#include "egw_err.inc"` 自动生成，**不手动同步**。
- 错误码按错误性质命名（`ERR_OPEN`、`ERR_INVALID_ARG`），不体现模块归属。
- 返回错误码时优先用 `EGW_RETURN_CODE(ERR_FOO)` 宏（展开为 `EGW_ERR_FOO`），见 `egw_defs.h:17`。

### 代码风格
- MISRA C:2012 Rule 15.6：`if`/`else`/`while`/`for` 体**必须**用花括号。允许 early return。
- 公共 API 前缀 `egw_`；不透明句柄 `egw_{module}_t`（不加 `_handle` 后缀）。
- **`.c` 文件**：用 `/* ── 主题 ── */` 分区线组织，函数不加注释（自解释命名）。禁止 `//` 行注释。
- **`.h` 公开头文件**：每个公开函数上方写 `/** @brief ... @param ... @return ... */` Doxygen 注释。可选文件级 `@file @brief` 块。
- **`.h` 内部头文件**（如 `test_uv_helper.h`）：仅分区线，不写 Doxygen。

### 模块化
- 新功能独立为 `src/xxx/` 静态库 + `include/` 公开头，不在已有模块内膨胀。
- Transport 是纯同步非阻塞 I/O 工具层（open/read/write/flush），不依赖 libuv，无回调、无状态机。
- App 通过 `egw_fsm_t` 状态机驱动，用 `egw_loop_t`（poll/timer/signal API）管理事件，不再直接调 `uv_*`。
- Protocol 以状态机驱动（`egw_proto_ctx_t`），App 推入字节后同步返回帧就绪/等更多/错误。

**尚未实现的设计**（以 CONTEXT.md 和 docs/design.md 为准）：

- **Lua 脚本引擎**：嵌入式脚本协程调度。（DS-004）
- **点表离线构建工具**：Python 脚本从 JSON/CSV 生成 `.bin`。（DS-003）
- **后台持久化线程**：独立线程周期性扫描脏页落盘。（DS-008）

### 配置 API
- 路径为 JSON Pointer（RFC 6901），如 `"/modbus/serial_ports/0/path"`。
- `egw_conf_load()` / `egw_conf_free()` 管理生命周期。
- `egw_conf_get_string()`, `egw_conf_get_int()`, `egw_conf_get_bool()`, `egw_conf_array_length()` 查值，键不存在时返回默认值。
- `egw_conf_enter()` 进入子树，后续查询以该子树为基准。
- `egw_conf_t` **不可跨线程共享**，每线程独立加载。

### 模块自注册
- `EGW_EXPORT(func, prio)` → `__attribute__((constructor(prio + 101)))`，在 `main()` 前执行。避开编译器保留区间 0–100。

### 第三方依赖
- 全部以 git submodule 放在 `third-party/`。用 `submodule-add <url> <path> [tag]` 添加，并在 `third-party/CMakeLists.txt` 注册。

## 线程化扩展约束

以下约束即便第一版单线程也必须遵守，防止未来引入线程时大范围重构（ADR-0010）。

### 线程分类
- **主线程**：持有 `egw_runtime_t`，处理 I/O、事件循环、数据分发。第一版只有一个。
- **单一功能线程**：不持有完整 runtime，只有专属资源 + `uv_async_t` 通信。例如持久化线程、Lua 计算线程。

### 约束
1. **禁止线程不安全的全局可变状态**：文件级 `static` 变量持有运行时数据、`strtok`/`strerror`/`localtime` 等非可重入函数。
   - 允许：只读配置、常量表、`pthread_once` 初始化的单例。
   - 运行时状态归 `egw_runtime_t` 或模块句柄。
2. **模块 API 必须可重入**：优先线程隔离模型（每线程独立句柄），不跨线程共享。mmap 映射等只读资源可跨线程。
3. **预期执行时间 >10ms 的功能**必须设计为可移出主线程：封装为纯函数 + 异步 API。
4. **阻塞 I/O**（`pwrite`、`fdatasync`、`sqlite3_step`）禁止在主事件循环线程执行。
5. **跨线程传递数据**时内存所有权必须清晰：所有权转移或 `_Atomic int ref_count`。

### 新增模块检查清单
- [ ] 无文件级 `static` 全局变量持有运行时数据
- [ ] 不使用 `strtok`、`strerror`、`localtime` 等非可重入函数（改用 `_r` 版本）
- [ ] 模块句柄通过显式参数传递，不通过全局变量
- [ ] 预期执行 >10ms 的功能设计为可移出主线程
- [ ] 跨线程传递的数据结构有清晰的所有权管理

## 参考

- `CONTEXT.md` — 领域术语表（统一语言）。
- `docs/design.md` — 设计决策记录。
- `docs/implementation.md` — 实施记录。
