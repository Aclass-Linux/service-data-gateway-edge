# DataGatewayHub — Agent Guide

项目文档以中文为主。优先读 `项目说明.md`、`项目计划.md`、`aclass.env.sh`。

## 构建与运行

```bash
source aclass.env.sh    # 提供 build / clean / install / rebuild / run 命令
build                   # cmake 配置 + 编译
run                     # 运行 ./build/src/app/gateway
clean                   # 删除 build/
rebuild                 # clean + build
```

`aclass.env.sh` 是可执行的真相源 — 所有构建流程以它为准，勿猜测。

## 项目状态

早期阶段，仅有 `src/app/main.c`（hello world 测试桩）。其余目录：
- `src/{core,protocol}/` — 空目录，M2+ 预留架构（见 `openspec/specs/architecture.md`）
- `tests/` — 空，尚未引入测试


## 技术栈

- C11（`_Noreturn`、`_Static_assert` 等 C11 特性使用时需标注 `/* C11 */`）
- CMake ≥ 3.20，`cmake -B build -DCMAKE_BUILD_TYPE=Debug`
- 目标平台：Linux

## 架构（见 `openspec/specs/architecture.md`）

```
app → protocol → connectors → core
```

当前只有 `app` 有实现。

## 目录结构要点

| 路径 | 说明 |
|---|---|
| `aclass.env.sh` | **入口** — source 后获得快捷命令 |
| `scripts/` | 独立脚本（build/clean/load），亦可直接调用 |
| `journal/` | 每日学习记录（markdown） |
| `openspec/` | 规范驱动变更工作流 |

## 避免踩坑

- 不要直接运行 `cmake` 而不用 `aclass.env.sh` — 路径和参数由脚本统一管理
- `src/{core,protocol}/` 是空目录，添加新源文件时需同时创建对应的 `CMakeLists.txt`
