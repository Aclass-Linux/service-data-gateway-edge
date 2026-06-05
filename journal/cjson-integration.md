# cJSON 集成方案

## 仓库结构

| 仓库 | 位置 | 用途 |
|------|------|------|
| `third-party/cjson-mirror` | 内网 GitLab | cJSON 源码镜像 + egw/ 改造分支 |
| `project/data-gateway` | 内网 GitLab | 主项目，submodule 引用 cjson-mirror |

---

## 内网仓库 remote 布局

```
origin      → git@gitlab.internal:third-party/cjson-mirror.git
upstream    → https://github.com/DaveGamble/cJSON.git（仅首次 fetch 需外网）
```

---

## 分支策略

```
内网 cjson-mirror 仓库：
  v1.7.17          ← 从上游手动同步的 tag
  v1.7.18          ← 从上游手动同步的 tag
  v1.7.19          ← 从上游手动同步的 tag
  egw/v1.7.18      ← 基于 v1.7.18，包含你的改动 ★
  egw/v1.7.19      ← 基于 v1.7.19，包含你的改动 ★
```

## 全流程操作

### 1. 内网 GitLab 建空仓库

项目路径 `third-party/cjson-mirror`，不初始化 README，空仓库。

### 2. 首次初始化（只需要一次外网）

```bash
# 完整克隆
git clone https://github.com/DaveGamble/cJSON.git
cd cJSON

# 将原始的 origin（GitHub）改名为 upstream
git remote rename origin upstream

# 加内网 remote 作为新的 origin
git remote add origin git@gitlab.internal:third-party/cjson-mirror.git

# 只推你需要的 tag 到内网
git push origin v1.7.18

# 创建你的改造分支
git checkout -b egw/v1.7.18 v1.7.18

# 改源码（裁剪功能、加配置读取等）...
# 提交
git add -A && git commit -m "egw: initial custom changes based on v1.7.18"

# 推改造分支到内网
git push -u origin egw/v1.7.18
```

### 3. 主项目引用

```bash
cd data-gateway

submodule-add \
    git@gitlab.internal:third-party/cjson-mirror.git \
    third-party/cjson \
    egw/v1.7.18
```

### 4. 日常修改 cJSON 代码

```bash
cd data-gateway/third-party/cjson

# 改代码
vim cJSON.c

git add -A
git commit -m "fix: xxx"
git push origin egw/v1.7.18

# 回到主项目
cd ../..
git add third-party/cjson
git commit -m "chore: update cjson submodule"
```

### 5. 上游发布新版本时的同步

```bash
# 5.1 拉取上游所有 tag（本地保持完整）
cd cjson-mirror
git fetch upstream --tags

# 5.2 只推需要的 tag 到内网
git push origin v1.7.19
```

### 6. CMake 集成

```cmake
# src/core/CMakeLists.txt
add_library(egw_core
    cJSON.c
    cJSON.h
    config.c config.h
    logger.c logger.h
)

target_include_directories(egw_core PUBLIC
    ${CMAKE_SOURCE_DIR}/third-party/cjson
)
```

cJSON 以源码形式编译进 egw_core 库，不依赖 cJSON 自带的 CMakeLists.txt。

---

## 总结用到的命令

```bash
# 同步上游（只需要两个命令）
git fetch upstream --tags       # 本地拉所有 upstream tag
git push origin v1.7.19         # 只推需要的 tag 到内网

# 创建改造分支
git checkout -b egw/v1.7.18 v1.7.18

# 搬移改动到新版本
git cherry-pick egw/v1.7.18..egw/v1.7.18
```

---

## 未来的第三方库（paho/mbedtls 等）

同一模式：
1. 内网 GitLab 建 `<name>-mirror` 空仓库
2. 本地 clone 上游 → 改 remote 推 tag 到内网
3. 加 upstream remote
4. 创建 egw/ 改造分支
5. 主项目 submodule-add 引用
