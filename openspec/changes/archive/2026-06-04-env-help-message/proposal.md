## Why

source aclass.env.sh 后没有可用命令提示，新开发者不知道有哪些命令可用。需在 source 时打印帮助信息，并提供 `help` 命令随时查看。

## What Changes

- source `aclass.env.sh` 时打印可用命令列表
- 新增 `help` 函数，打印与 source 时相同的内容
- 打印内容包含所有可用命令及其简短说明

## Capabilities

### New Capabilities
- `env-help`: source 时自动打印可用命令列表，并提供 help 命令随时查看

### Modified Capabilities

（无）

## Impact

- `aclass.env.sh` — 新增 help 函数和 source 时的打印逻辑