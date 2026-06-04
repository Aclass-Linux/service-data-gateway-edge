## Context

当前 `submodule-config-management` 实现使用 `.gitmodules` 的 `tag` 字段管理版本锁定。实践发现 `.gitmodules` 是 git 内部文件，对其做非标准修改会导致 `git rm -f` 拒绝、`git submodule update` 报错等问题。改为独立的 `.project.submodules` 文件，`git config -f` 解析，完全不干涉 git 内部状态。

## Goals / Non-Goals

**Goals:**
- `.project.submodules` 作为 submodule 依赖唯一真相源
- `submodule-add` 写 `.project.submodules` + `git submodule add`（分离职责）
- `submodule-rm` 清 `.project.submodules` + `git submodule deinit/rm`
- `submodule-sync` 按 `.project.submodules` 同步
- `.gitmodules` 回归纯 git 管理，不加自定义字段

**Non-Goals:**
- 不修改 git 对 `.gitmodules` 的自动管理行为
- 不改变 build/submodule 的命令接口

## Decisions

### Decision 1: 用 `.project.submodules` 替代 `.gitmodules` tag

```ini
# .project.submodules（提交 git）
[submodule "paho-mqtt-c"]
    url = https://github.com/eclipse/paho.mqtt.c.git
    path = third-party/paho-mqtt-c
    tag = v1.3.13
```

`git config -f .project.submodules` 读写，与 git 内部机制完全隔离。

### Decision 2: submodule-add 职责分离

```bash
_dgh_submodule_add() {
    # 1. 写 .project.submodules（我们管）
    $_DGH_GIT config -f .project.submodules submodule.$name.url "$url"
    $_DGH_GIT config -f .project.submodules submodule.$name.path "$path"
    [ -n "$tag" ] && $_DGH_GIT config -f .project.submodules submodule.$name.tag "$tag"

    # 2. git submodule add（git 管）
    $_DGH_GIT submodule add --name "$name" "$url" "$path"
    [ -n "$tag" ] && $_DGH_GIT -C "$path" checkout "$tag"
}
```

`.project.submodules` 和 `.gitmodules` 各自管理各自的内容，互不干扰。

### Decision 3: submodule-rm 清理两份

```bash
_dgh_submodule_rm() {
    # 1. 清 .project.submodules
    $_DGH_GIT config -f .project.submodules --remove-section submodule.$name

    # 2. git submodule deinit/rm
    $_DGH_GIT submodule deinit -f "$path"
    $_DGH_GIT rm -f "$path"
}
```

`.project.submodules` 清理不受 git 状态影响（独立文件）。
`.gitmodules` 由 `git rm -f` 自动清理。

### Decision 4: submodule-sync 从 .project.submodules 读

双向同步逻辑不变，读取源从 `.gitmodules` 改为 `.project.submodules`。

### Decision 5: .project.submodules 提交 git

与 `.project.config` 同级，作为项目公共配置提交。

## Risks / Trade-offs

- [两份配置] `.project.submodules` 和 `.gitmodules` 共存，需保持同步 → `submodule-add`/`rm` 同时更新两份，手动操作时需留意
- [新人认知] 多一个配置文件，需要理解两者关系 → 文档说明 `.gitmodules` 是 git 产物，`.project.submodules` 是项目依赖定义