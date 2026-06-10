# AGENTS.md

## 构建

所有构建必须先加载环境：

```bash
source aclass.env.sh
```

提供以下命令：`build`, `release`, `clean`, `rebuild`, `run`, `submodule-add`, `submodule-rm`, `submodule-sync`

- `build` — cmake 配置 + 编译（依赖 Ninja，必须安装 `ninja`）
- `release` — 设置 `CMAKE_BUILD_TYPE=Release`，编译，安装到 `install/`
- `run` — 运行网关二进制（仅 x86_64；armv7 会拒绝执行）。默认使用 `config.json`，自定义路径加 `-c <path>`。
- 修改 `.project.config` 中的 `ARCH` 后，下次构建自动清除 `build/`

测试框架：Unity。构建后从 `build/` 运行 `ctest`（或直接执行 `build/bin/test_transport`、`build/bin/test_serial`）。无 lint 或类型检查命令。

## 配置

- `.project.config` — 公共构建参数（已提交）。每行 `KEY=VAL` 会被自动提取并传给 CMake 作为 `-DKEY=VAL`。
- `.project.local.config` — 本地覆盖（gitignored）。交叉编译工具链的 `COMPILE_PATH` / `SYSROOT_PATH` 写在这里。本地配置先加载，同名变量会被公共配置覆盖。
- `ARCH` 控制工具链选择：`x86_64` → `cmake/toolchain-x86_64.cmake`，`armv7` → `cmake/toolchain-armv7.cmake`。
- 运行时默认加载 `config.json`（项目根目录）。`run -c <path>` 指定其他路径。

## 项目结构

```
src/app/         → 可执行入口 (main.c)，目标名 = EdgeGateWay
src/core/        → egw_core 静态库 (config.c, egw_defs.h)
  include/       → 公开头文件 (config.h, egw_defs.h)
src/transport/   → egw_transport 静态库 (egw_transport.c, egw_serial.c)
  include/       → 公开头文件 (egw_transport.h, egw_serial.h)
third-party/     → git 子模块 (cjson, libuv, unity)
cmake/           → AClass.cmake（公共 CMake 设置）、工具链文件
scripts/         → build.sh, release.sh, clean.sh, submodule.sh
tests/           → Unity 测试（test_transport, test_serial）
```

首次克隆后需同步子模块：`submodule-sync`

## 约定

- 严格 C11（`CMAKE_C_EXTENSIONS OFF`）。仅 GCC/Clang（`-Wall -Wextra`；Release 模式 `-Werror`）。
- 模块自注册：`EGW_EXPORT(func, prio)` 通过 `__attribute__((constructor))` 让函数在 `main()` 之前执行。内部优先级偏移 +101，避开编译器保留区间 0–100。
- 错误码：`egw_err_t`（int32_t）。`EGW_OK = 0`，负值为错误（`EGW_ERR_FILE_NOT_FOUND`, `EGW_ERR_PARSE` 等）。新增错误码追加在末尾，保持现有值不变。
- 配置 API 使用 JSON Pointer（RFC 6901）路径：`"/modbus/serial_ports/0/path"`。优先使用便捷宏 `EGW_CONF_STR()`, `EGW_CONF_INT()`, `EGW_CONF_BOOL()`, `EGW_CONF_ARR_LEN()` 而非直接调用函数。
- 编译产物输出到 `build/bin/`（可执行）和 `build/lib/`（库）。安装目标是 `install/`。
- 新增第三方依赖：以 git submodule 形式放在 `third-party/` 下；用 `submodule-add <url> <path> [tag]` 添加，并在 `third-party/CMakeLists.txt` 中注册。
- Transport 层基于 libuv，全部操作异步回调（open/data/write/close 均为回调），多态通过 vtable 实现。
