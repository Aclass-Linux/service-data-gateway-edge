## Purpose

定义 submodule 依赖的配置管理方式：通过 `.project.submodules` 独立文件管理 url、path、tag。

## Requirements

### Requirement: .project.submodules 定义依赖

`.project.submodules` SHALL 使用 INI 格式（兼容 `git config -f`），每项 submodule 包含 url、path、tag（可选）。

#### Scenario: 文件提交 git

- **WHEN** 添加或修改 `.project.submodules`
- **THEN** 文件被 git 跟踪（提交）
- **AND** 作为项目依赖的唯一真相源

### Requirement: submodule-add 写入 .project.submodules

`submodule-add` SHALL 操作 `.project.submodules`，不碰 `.gitmodules`。

#### Scenario: 带 tag 添加

- **WHEN** 执行 `submodule-add <url> <path> <tag>`
- **THEN** 用 `git config -f .project.submodules` 写入 url、path、tag
- **AND** 执行 `git submodule add`（纯 git 操作，不修改 `.gitmodules` 之外的内容）
- **AND** checkout 到指定 tag

#### Scenario: 无 tag 添加

- **WHEN** 执行 `submodule-add <url> <path>`
- **THEN** 写入 url 和 path
- **AND** 不写入 tag

### Requirement: submodule-rm 清理 .project.submodules

`submodule-rm` SHALL 清理 `.project.submodules` 中的对应段落。

#### Scenario: 删除子模块

- **WHEN** 执行 `submodule-rm <path>`
- **THEN** 用 `git config -f .project.submodules --remove-section` 删除段落
- **AND** 执行 `git submodule deinit -f` + `git rm -f`

### Requirement: submodule-sync 基于 .project.submodules

`submodule-sync` SHALL 读取 `.project.submodules` 进行同步。

#### Scenario: 补齐缺失

- **WHEN** `.project.submodules` 中某 submodule 本地缺失
- **THEN** `git submodule add` 拉取
- **AND** 有 tag 则 checkout

#### Scenario: 询问删除多余

- **WHEN** 本地 `third-party/` 下目录不在 `.project.submodules` 中
- **THEN** 逐个询问后删除