# DataGatewayHub — Agent Guide

项目文档以中文为主。优先读 `项目说明.md`、`项目计划.md`、`.project.config`。

## 构建与运行

```bash
source aclass.env.sh    # 加载配置，提供 build/release/clean/rebuild/run/submodule-* 命令
build                   # cmake 配置 + 编译（产物在 build/bin/ + build/lib/）
release                 # build(Release) + cmake --install → install/
run                     # 运行 build/bin/gateway（仅 x86_64）
clean                   # 删除 build/
rebuild                 # clean + build
submodule-add <url> <path>  # 添加 git submodule
submodule-rm <path>         # 删除 git submodule
submodule-sync              # 同步所有 submodule
```

配置通过 `.project.config`（公共）和 `.project.local.config`（本地覆盖）控制。
`scripts/` 下脚本可独立运行，不依赖 `aclass.env.sh`。

## 项目状态

早期阶段，仅有 `src/app/main.c`（hello world 测试桩）。其余目录：
- `src/{core,protocol}/` — 空目录，M2+ 预留架构（见 `openspec/specs/architecture.md`）
- `tests/` — 空，尚未引入测试

## 技术栈

- C11（`_Noreturn`、`_Static_assert` 等 C11 特性使用时需标注 `/* C11 */`）
- CMake ≥ 3.20，通过 `.project.config` + `cmake/toolchain-*.cmake` 管理配置
- 目标平台：Linux（x86_64 / armv7）
- 第三方依赖：git submodule（third-party/）

## 配置

| 文件 | 用途 | 是否提交 |
|---|---|---|
| `.project.config` | 公共配置：ARCH、DGH_LINK、CMAKE_BUILD_TYPE | 是 |
| `.project.local.config` | 本地覆盖：CROSS_COMPILE_PATH、SYSROOT_PATH | 否（.gitignore） |

## 架构（见 `openspec/specs/architecture.md`）

```
app → protocol → connectors → core
```

当前只有 `app` 有实现。

## 目录结构要点

| 路径 | 说明 |
|---|---|
| `aclass.env.sh` | **入口** — source 后获得快捷命令，委托 scripts/ |
| `.project.config` | 公共构建配置（ARCH/CMAKE_BUILD_TYPE/DGH_LINK） |
| `.project.local.config` | 本地构建覆盖（工具链路径） |
| `scripts/` | 独立可执行脚本（build/clean/release/toolchain/submodule） |
| `cmake/` | CMake 工具链文件（toolchain-x86_64 / toolchain-armv7） |
| `third-party/` | git submodule 第三方依赖 |
| `build/` | Debug 产物（bin/ + lib/） |
| `install/` | Release 产物（bin/ + lib/） |
| `journal/` | 每日学习记录（markdown） |
| `openspec/` | 规范驱动变更工作流 |

## 避免踩坑

- 不要直接运行 `cmake` 而不用 `aclass.env.sh` — 路径和参数由脚本统一管理
- `src/{core,protocol}/` 是空目录，添加新源文件时需同时创建对应的 `CMakeLists.txt`
- 切换 ARCH 时 build 会自动检测并清理重建
- `release` 强制使用 CMAKE_BUILD_TYPE=Release，忽略配置文件中的设置
- ARM 架构下 `run` 命令不可用