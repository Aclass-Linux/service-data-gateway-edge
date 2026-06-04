## Context

当前 `aclass.env.sh` 被 source 后无任何提示，开发者需要查看源码才能知道有哪些命令可用。这对新加入的开发者不友好。

## Goals / Non-Goals

**Goals:**
- source `aclass.env.sh` 后自动打印可用命令列表
- 提供 `help` 命令打印相同内容

**Non-Goals:**
- 不修改各命令的参数或行为
- 不引入彩色输出或复杂格式

## Decisions

### Decision 1: 帮助信息内容

打印内容包含所有 shell 函数名及其一行说明：

```
Available commands:
  build              cmake 配置 + 编译
  release            编译 Release 并安装到 install/
  clean              清理构建产物
  rebuild            clean + build
  run                运行网关程序（x86_64 only）
  submodule-add      添加 git submodule
  submodule-rm       删除 git submodule
  submodule-sync     同步所有 submodule
  help               显示此帮助信息
```

### Decision 2: 打印时机

source 时无条件打印。理由：命令不多（8 条），打印开销可忽略，且确保开发者始终知道有哪些命令。

### Decision 3: help 函数复用同一内容

help 函数调用 `_dgh_help()` 内部函数，source 末尾也调用 `_dgh_help()`，避免内容重复。

## Risks / Trade-offs

- [打印噪声] 每次 source 都打印可能打扰熟练用户 → 内容简洁（<10 行），可接受