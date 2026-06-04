## Purpose

> **已废弃** — 此方案（`.gitmodules` 自定义 `tag` 字段）已被 `submodule-config-file` 方案取代（`.project.submodules` 独立文件）。

定义 submodule 配置条目的初始管理方式，通过 `.gitmodules` 自定义 `tag` 字段进行版本锁定。

## Requirements

### Requirement: submodule 配置条目管理

`.gitmodules` SHALL 包含自定义 `tag` 字段用于版本锁定。

#### Scenario: submodule-add 写入 tag

- **WHEN** 执行 `submodule-add <url> <path> <tag>`
- **THEN** `git submodule add` 写入 url 和 path
- **AND** `git config -f .gitmodules` 追加 tag 字段
- **AND** checkout 到指定 tag

#### Scenario: submodule-add 无 tag

- **WHEN** 执行 `submodule-add <url> <path>`
- **THEN** `git submodule add` 写入 url 和 path
- **AND** 不追加 tag 字段，不做版本锁定

#### Scenario: submodule-rm 清理 tag

- **WHEN** 执行 `submodule-rm <path>`
- **THEN** `git rm -f` 删除 `.gitmodules` 中整段（含 tag 字段）
- **AND** 清理工作区和 `.git/modules/` 缓存

### Requirement: submodule-sync 双向同步

`submodule-sync` SHALL 按 `.gitmodules` 补充缺失的 submodule，并清理多余目录。

#### Scenario: 补齐缺失

- **WHEN** `.gitmodules` 中某 submodule 本地缺失
- **THEN** `git submodule update --init --recursive`
- **AND** 有 tag 且不匹配时 `git checkout <tag>`

#### Scenario: 询问删除多余

- **WHEN** 本地 `third-party/` 下目录不在 `.gitmodules` 中
- **THEN** 逐个询问 "delete `<path>`? [y/N]"
- **AND** 确认则 `git submodule deinit -f <path>` + `git rm -f <path>` + 清理缓存

### Requirement: build submodule 缺失提示

`build()` SHALL 在 cmake 失败时检测 submodule 缺失。

#### Scenario: cmake 失败 + submodule 缺失

- **WHEN** `cmake` 命令失败
- **AND** 有 `.gitmodules` 中定义的 submodule 路径缺失
- **THEN** 打印 `Tip: Try 'submodule-sync' to sync submodules`