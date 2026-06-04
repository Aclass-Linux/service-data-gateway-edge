## Why

当前 submodule 依赖需要手动输入 URL、路径、tag 来添加，新开发者克隆后不知道需要拉取哪些依赖。把 submodule 配置统一写进 `.gitmodules`（追加自定义 `tag` 字段），`submodule-add`/`submodule-rm`/`submodule-sync` 自动管理配置条目，降低手动操作成本和新人上手门槛。

## What Changes

- `.gitmodules` 追加自定义 `tag` 字段，作为版本锁定依据
- 重写 `submodule-add`：写入 `.gitmodules` 并追加 tag（可选）
- 重写 `submodule-rm`：从 `.gitmodules` 清理整段（含 tag）
- 重写 `submodule-sync`：双向同步——按 `.gitmodules` 补缺失，遍历本地目录逐个询问删除多余
- `build()` 在 cmake 失败时检测是否因 submodule 缺失导致，是则提示

## Capabilities

### New Capabilities
- `submodule-config`: 通过 `.gitmodules` 的 tag 字段统一管理 submodule 依赖，命令自动维护配置条目

### Modified Capabilities

（无）

## Impact

- `scripts/submodule.sh` — 重写，改用 `git config -f .gitmodules` 读写 tag
- `scripts/build.sh` — 加 cmake 失败时 submodule 缺失检测提示
- `scripts/toolchain.sh` — `_dgh_check_submodules` 改为读 `.gitmodules`
- `.gitmodules` — 追加 tag 字段（已有文件则更新）