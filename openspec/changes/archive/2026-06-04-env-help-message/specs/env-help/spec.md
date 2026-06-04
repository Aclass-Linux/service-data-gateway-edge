## ADDED Requirements

### Requirement: source 时打印帮助信息

`aclass.env.sh` 被 source 时 SHALL 打印可用命令列表。

#### Scenario: 首次 source

- **WHEN** 执行 `source aclass.env.sh`
- **THEN** 打印所有可用命令及其简短说明
- **AND** 列表包含 build、release、clean、rebuild、run、submodule-add、submodule-rm、submodule-sync、help

### Requirement: help 命令

`aclass.env.sh` SHALL 提供 `help` 函数，打印与 source 时相同的命令列表。

#### Scenario: 调用 help

- **WHEN** 执行 `help`
- **THEN** 打印与 source 时完全相同的命令列表