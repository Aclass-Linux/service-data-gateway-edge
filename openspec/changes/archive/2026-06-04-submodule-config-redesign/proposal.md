## Why

当前方案在 `.gitmodules` 中追加自定义 `tag` 字段，但 `.gitmodules` 是 git 内部管理的文件，对其进行任何非标准修改都会导致 git 操作冲突：

- `git rm -f` 拒绝：`.gitmodules` 有未暂存修改时拒绝执行
- `git submodule update` 报错：找不到自定义字段对应的 URL
- `.gitmodules` 状态冲突：stage 状态和 git 隐式操作相互干扰

改为独立文件 `.project.submodules`，用 `git config -f` 解析，完全不受 git 内部机制干涉。

## What Changes

- 新增 `.project.submodules` 文件，使用 INI 格式存储 submodule 依赖（url、path、tag）
- `submodule-add` / `submodule-rm` / `submodule-sync` 读写 `.project.submodules`，不再碰 `.gitmodules`
- `.project.submodules` 提交 git（作为项目依赖定义）
- 删除 `.gitmodules` 中的自定义 `tag` 字段逻辑（`.gitmodules` 回归纯 git 管理）

## Capabilities

### New Capabilities
- `submodule-config-file`: 通过 `.project.submodules` 独立文件管理 submodule 依赖，避免与 git 内部机制冲突

### Modified Capabilities

（无）

## Impact

- `.project.submodules` — 新增，项目依赖定义（提交 git）
- `scripts/submodule.sh` — 读写 `.project.submodules` 替代 `.gitmodules` tag
- `scripts/toolchain.sh` — `_dgh_check_submodules` 读 `.project.submodules` 替代 `.gitmodules`
- `.gitignore` — 不忽略 `.project.submodules`
- `openspec/specs/submodule-config/spec.md` — 更新为使用独立文件