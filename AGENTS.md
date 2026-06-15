# AGENTS.md

## 构建

所有构建必须先加载环境：

```bash
source aclass.env.sh
```

提供命令：`build`, `release`, `clean`, `rebuild`, `run`, `submodule-add`, `submodule-rm`, `submodule-sync`, `help`

- `build` — cmake 配置（Ninja 生成器）+ 编译。**必须安装 `ninja`**。
- `release` — 设 `CMAKE_BUILD_TYPE=Release`，编译，安装到 `install/`。
- `clean` — 删除 `build/`。
- `run` — 运行网关二进制（仅 x86_64；armv7 会拒绝执行）。`-c <path>` 指定配置文件，默认 `config.json`。`-c` 由二进制自身解析，`run` 函数只是透传参数。
- 修改 `.project.config` 中的 `ARCH` 后，下次构建自动清除 `build/`（通过 `.build_arch` 标记检测）。

测试框架：Unity。`ACLASS_ENABLE_TESTS=ON` 控制。构建后在 `build/` 运行 `ctest`，或直接执行 `build/bin/test_transport`、`build/bin/test_serial`。无 lint/类型检查命令。

## 配置

- `.project.config` — 公共构建参数（已提交）。每行 `KEY=VAL` 自动作为 `-DKEY=VAL` 传给 CMake。变量 `ACLASS_PROJECT_NAME` 控制输出二进制名称（默认 `AClassDemo_${ARCH}`，本项目设为 `EdgeGateWay`）。
- `.project.local.config` — 本地覆盖（gitignored）。交叉编译时在这里写 `COMPILE_PATH` / `SYSROOT_PATH`。本地配置**先**加载，同名变量会被 `.project.config` 覆盖。
- `ARCH` 控制工具链：`x86_64` → `cmake/toolchain-x86_64.cmake`，`armv7` → `cmake/toolchain-armv7.cmake`。
- 所有 Linux 目标自动定义 `_GNU_SOURCE`（`cmake/toolchain-linux-common.cmake`）。

## 项目结构

```
src/app/         → 可执行入口 (main.c, gateway_app.c)，链接 egw_core + egw_transport + egw_protocol
src/core/        → egw_core 静态库 (egw_err.c, config.c)
  include/       → 公开头文件 (egw_defs.h, egw_err.inc, config.h)
src/transport/   → egw_transport 静态库 (egw_transport.c, egw_serial.c)
  include/       → 公开头文件 (egw_transport.h, egw_serial.h, egw_serial_params.h)
src/protocol/    → egw_protocol 静态库 (egw_protocol.c, egw_protocol.h)，当前为桩
third-party/     → git 子模块 (cjson, libuv, unity)
cmake/           → AClass.cmake（公共 CMake 设置）、工具链文件
scripts/         → build.sh, release.sh, clean.sh, submodule.sh
tests/           → Unity 测试（transport_sim/ 下 test_transport, test_serial）
```

首次克隆后需同步子模块：`submodule-sync`

## 约定

### 语言与编译器
- 严格 **C11**（`CMAKE_C_EXTENSIONS OFF`）。仅 GCC/Clang。
- Debug：`-Wall -Wextra`。Release：额外 `-Werror`。

### 错误码
- 类型 `egw_err_t` = `int32_t`。`EGW_OK = 0`，负值为错误。
- 通过 **X-macro 表** `src/core/include/egw_err.inc` 定义，而非直接写 enum。新增错误码：
  1. 在 `egw_err.inc` 末尾追加 `EGW_ERROR_CODE(ERR_FOO, -N, "description")`
  2. 枚举和 `egw_err_str()` 通过 `#include "egw_err.inc"` 自动生成，无需手动同步。

### 代码风格
- MISRA C:2012 Rule 15.6：所有 `if`/`else`/`while`/`for` 体必须用花括号包裹，禁止裸语句。Early return 允许。
- 公共 API 前缀 `egw_`。不透明句柄命名：`egw_{module}_t`（不加 `_handle` 后缀）。

### 注释规范
- **`.c` 源文件**：用 `/* ── 主题 ── */` 分区线组织代码，每区一组相关函数。函数本身不加注释（命名自解释）。禁止 `//` 行内注释。
- **`.h` 公开头文件**：文件头部可选 `/** @file ... @brief ... */` Doxygen 块说明模块职责。内部用 `/* ── 主题 ── */` 分区。每个公开函数上方必须写 `/** @brief ... @param ... @return ... */` Doxygen 注释（风格见 `src/core/include/config.h`）。
- **`.h` 内部头文件**（如 `egw_transport_internal.h`）：仅用 `/* ── 主题 ── */` 分区，不写 Doxygen。

### 模块化与扩展性
- 新功能优先独立为新的静态库（`src/xxx/` + `src/xxx/CMakeLists.txt`），不在已有模块内膨胀。
- 公开 API 通过 `include/` 子目录暴露，`.c` 和内部头留在模块目录。
- 设计时预留扩展点：用 vtable 实现多态、用 `void *user_data` 传递上下文、用数组+容量代替固定长度、枚举值预留区间。
- 新增跨模块功能时优先使用现有机制（Channel/PubSub、EGW_EXPORT 自注册、egw_err_t 错误码），不另起一套。

### 模块自注册
- `EGW_EXPORT(func, prio)` 宏利用 `__attribute__((constructor))` 在 `main()` 前执行函数。内部优先级 +101 偏移，避开编译器保留区间 0–100（定义见 `src/core/include/egw_defs.h:25-31`）。

### 配置 API
- 使用 JSON Pointer（RFC 6901）路径：`"/modbus/serial_ports/0/path"`。
- 直接调用 `egw_conf_get_string()`, `egw_conf_get_int()`, `egw_conf_get_bool()`, `egw_conf_array_length()`。
- `egw_conf_t` 不可跨线程共享；每线程独立加载。

### 编译产物
- 输出到 `build/bin/`（可执行）和 `build/lib/`（库）。安装目标 `install/`。

### 第三方依赖
- 以 git submodule 形式放在 `third-party/` 下。用 `submodule-add <url> <path> [tag]` 添加，并在 `third-party/CMakeLists.txt` 中注册。

### Transport 层
- 基于 libuv，全部操作异步回调（open/data/write/close）。多态通过 vtable（函数指针表）实现。
- `egw_transport_internal.h` — 传输层内部结构体，外部模块不应直接包含。

## 参考

- `CONTEXT.md` — 领域模型与术语表（Domain Model / Ubiquitous Language）。
- `docs/adr/` — 架构决策记录。
