## Context

当前 submodule 依赖完全靠手动输入 URL、路径、tag 管理，没有中心化的配置入口。新人 clone 后不知道缺哪些依赖，`build()` 也不会提示。需将 submodule 配置整合到 `.gitmodules`（追加自定义 `tag` 字段），让命令自动维护这块配置。

## Goals / Non-Goals

**Goals:**
- `.gitmodules` 作为 submodule 依赖的唯一真相源
- `submodule-add` 自动写入 `.gitmodules`（含 tag）
- `submodule-rm` 自动清理 `.gitmodules`（含 tag）
- `submodule-sync` 双向同步：按 `.gitmodules` 补缺失，多余目录逐个询问删除
- `build()` cmake 失败时检测 submodule 缺失并提示

**Non-Goals:**
- 不添加新的配置文件（一切在 `.gitmodules`）
- 不实现架构条件依赖（如 "armv7 才需要此 submodule"）
- `build()` 不主动拉取 submodule，只提示

## Decisions

### Decision 1: 用 `git config -f .gitmodules` 读写 tag

`git config -f .gitmodules` 是 INI 读写器，不校验字段名。`tag` 作为自定义字段可正常读写，git 静默忽略。

```bash
# 写
git config -f .gitmodules submodule.paho-mqtt-c.tag v1.3.13

# 读
tag=$(git config -f .gitmodules submodule.paho-mqtt-c.tag || true)

# 删（段落级）
git config -f .gitmodules --remove-section submodule.paho-mqtt-c
```

### Decision 2: submodule name 从 path basename 推导

```
third-party/paho-mqtt-c → name: paho-mqtt-c
third-party/mbedtls     → name: mbedtls
```

`git submodule add --name <name>` 确保 section 名可控。

### Decision 3: submodule-sync 双向同步

方向一（`.gitmodules` → 本地）：
```
遍历 .gitmodules
  ├─ 本地缺失 → git submodule update --init
  └─ tag 不匹配 → git checkout <tag>
```

方向二（本地 → `.gitmodules`）：
```
扫描 third-party/*/（有 submodule 标记的目录）
  ├─ 在 .gitmodules 中 → 跳过
  └─ 不在 .gitmodules 中 → 逐个询问确认后删除
```

### Decision 4: build 只提示不自动拉

`build()` 在 cmake 配置前记录缺失的 submodule 列表。cmake 失败时检查该列表：

```bash
cmake ... || {
    $missing && echo "Tip: Try 'submodule-sync' to sync submodules"
    return 1
}
```

### Decision 5: toolchain.sh 替换 glob 读 .gitmodules

`_dgh_check_submodules` 原用 shell glob 扫描 `third-party/*/`（zsh 下不匹配报错），改为读 `.gitmodules` 判断 submodule 是否已拉取。

## Risks / Trade-offs

- [section 名冲突] 两个 submodule 的 path basename 相同时 `--name` 冲突 → path 设计时需避免 basename 重复
- [git config 依赖] 极端老旧 git 版本可能不支持 `-f` → 当前环境 git ≥ 2.0，无此问题