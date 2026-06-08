# DataGatewayHub — Agent Guide

项目文档以中文为主。优先读 `项目说明.md`、`项目计划.md`、`.project.config`。

## 构建与运行

```bash
source aclass.env.sh    # 必须 source，提供 build/release/clean/rebuild/run/submodule-* 命令
build                   # cmake 配置 + Ninja 编译（产物在 build/bin/EdgeGateWay）
release                 # build(Release) + cmake --install → install/
run                     # 运行 build/bin/EdgeGateWay（仅 x86_64）
clean                   # 删除 build/
rebuild                 # clean + build
submodule-add/sync/rm   # git submodule 管理
```

配置通过 `.project.config`（公共）和 `.project.local.config`（本地覆盖）控制。`scripts/` 下脚本可直接运行。

## 当前实现状态

| 层 | 路径 | 状态 |
|---|---|---|
| `app` | `src/app/main.c` | ✅ 加载 config.json（-c 参数），打印 MQTT/Modbus 配置 |
| `core` | `src/core/` | ✅ 配置框架（cJSON key-path 查询）、错误码（egw_err_t）、模块自动初始化宏（EGW_EXPORT） |
| `protocol` | `src/protocol/` | ⚪ 空目录 |
| `connectors` | `src/connectors/` | ⚪ 空目录 |

构建产物命名由 `.project.config` 中 `ACLASS_PROJECT_NAME` 控制（当前：`EdgeGateWay`）。

## 依赖

- `third-party/cjson` — git submodule，由 `build` 自动同步
- CMake 中 `src/core` → 静态库 `egw_core` → 链接给 `src/app`
- 新增子模块时 `.gitmodules` 和 `.project.submodules` 两个文件需保持同步

## 技术栈

- C11（`_Noreturn`、`_Static_assert` 等 C11 特性使用时标注 `/* C11 */`）
- CMake ≥ 3.20，通过 `.project.config` + `cmake/toolchain-*.cmake` 管理交叉编译
- 目标平台：Linux x86_64 / armv7
- 第三方：git submodule（当前仅 `cjson`）

## 避免踩坑

- 不要直接运行 `cmake` — 路径和参数由 `aclass.env.sh` 统一管理
- 切换 ARCH 时 build 会自动检测并清理重建
- `release` 强制 `CMAKE_BUILD_TYPE=Release`，忽略配置文件
- ARM 下 `run` 不可用
- `src/{core,protocol}/` 添加新源文件时需同时更新对应 `CMakeLists.txt`
- `src/protocol/` 和 `src/connectors/` 尚未创建 CMakeLists.txt，新目录需加 `add_subdirectory`
- 主 `CMakeLists.txt` 中 `cmake/toolchain-${ARCH}.cmake` 和 `include(cmake/AClass.cmake)` 的顺序必须保持不变

## 与 Git 交互

- `build/`、`install/`、`.project.local.config` 在 `.gitignore` 中
- 未跟踪的新文件须 `git add` 后再提交
