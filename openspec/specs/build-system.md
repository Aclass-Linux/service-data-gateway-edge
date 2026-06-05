# Build System

## 配置系统

### 双层配置

| 文件 | 用途 | 是否提交 |
|---|---|---|
| 文件 | 用途 | 是否提交 |
|---|---|---|
| `.project.config` | 公共配置：ARCH、`*_LIB_MODE`、CMAKE_BUILD_TYPE、ACLASS_PROJECT_NAME | 是 |
| `.project.local.config` | 本地覆盖：COMPILE_PATH、SYSROOT_PATH | 否（.gitignore） |

#### 配置加载顺序

- **WHEN** `aclass.env.sh` 被 source
- **THEN** 先加载 `.project.local.config` 设置个人默认值
- **AND** 再加载 `.project.config`（若存在）
- **AND** 后者覆盖前者同名变量

#### 默认值

- **WHEN** `.project.config` 不存在
- **THEN** 使用硬编码默认值（ARCH=x86_64, ACLASS_LIB_MODE=SHARED, CMAKE_BUILD_TYPE=Debug）

#### 架构切换

- **WHEN** 修改 ARCH 值
- **THEN** 下次 `build` 时检测到架构变更（通过 `.build_arch` 标记文件）
- **AND** 自动清理 build/ 目录后重新编译

---

## 构建命令

### Build

- **WHEN** 执行 `build`
- **THEN** cmake 配置 + 编译，产物输出到 `build/bin/` 和 `build/lib/`
- **AND** 所有配置变量（ARCH、`*_LIB_MODE` 等）从 `.project.config` / `.project.local.config` 自动传入 cmake
- **AND** 设 BUILD_RPATH=$ORIGIN/../lib

### Release

- **WHEN** 执行 `release`
- **THEN** 先执行 build，再 cmake --install 到 `install/`
- **AND** 输出可执行文件和 `lib/*.so`
- **AND** 设 INSTALL_RPATH=$ORIGIN/../lib
- **AND** 每次 release 先 `rm -rf install/` 再 install

### Clean / Rebuild

- **WHEN** 执行 `clean` → 删除 build/
- **WHEN** 执行 `rebuild` → clean + build

### 独立运行

- **WHEN** 执行 `./scripts/build.sh`
- **THEN** 自加载配置，行为与 `source aclass.env.sh && build` 一致

### 命令列表

- **WHEN** 执行 `source aclass.env.sh`
- **THEN** 打印可用命令列表
- **AND** `help` 命令打印相同内容

---

## 工具链

工具链选择由 `AClass.cmake` 在 `project()` 之前根据 `ARCH` 自动完成。

| ARCH | 工具链文件 | 编译器来源 |
|------|-----------|--------|
| x86_64 | `cmake/toolchain-x86_64.cmake` | 系统默认或 COMPILE_PATH（可选） |
| armv7 | `cmake/toolchain-armv7.cmake` | COMPILE_PATH（必填） |

#### 编译器检查

- **WHEN** `ARCH=armv7` 且 `COMPILE_PATH` 未设置
- **THEN** CMake 报错要求配置 `.project.local.config`

#### ARM 运行保护

- **WHEN** `ARCH=armv7` 时执行 `run`
- **THEN** 报错提示用户部署到目标板或切回 x86_64

---

## 编译选项

### 优化等级

- **WHEN** CMAKE_BUILD_TYPE 未设置 → 默认 Debug（-O0 -g）
- **WHEN** 执行 `release` → 强制 Release（-O3 -DNDEBUG），忽略配置文件
- **WHEN** 执行 `CMAKE_BUILD_TYPE=RelWithDebInfo build` → 使用命令行传入的值

### 链接方式

- **WHEN** ACLASS_LIB_MODE=SHARED → 动态链接
- **WHEN** ACLASS_LIB_MODE=STATIC → 静态链接
- **WHEN** ACLASS_LIB_MODE=OBJECT → 仅编译目标文件，符号合入最终目标

---

## 产物目录

| 类型 | 编译产物 | 安装产物 |
|------|---------|---------|
| 可执行文件 | `build/bin/` | `install/bin/` |
| 库文件(.so/.a) | `build/lib/` | `install/lib/` |

---

## Submodule 管理

### .project.submodules

项目通过 `.project.submodules`（INI 格式，git 跟踪）管理第三方依赖。

```
[submodule "paho-mqtt-c"]
    url = https://github.com/eclipse/paho.mqtt.c.git
    path = third-party/paho-mqtt-c
    tag = v1.3.13
```

### submodule-add

- **WHEN** 执行 `submodule-add <url> <path> <tag>`
- **THEN** 写入 `.project.submodules`（url/path/tag）
- **AND** 执行 `git submodule add` checkout 到指定 tag
- **WHEN** 无 tag → 不锁定版本

### submodule-rm

- **WHEN** 执行 `submodule-rm <path>`
- **THEN** 清 `.project.submodules` 对应段落
- **AND** `git submodule deinit -f` + `git rm -f` 清理

### submodule-sync

- **WHEN** `.project.submodules` 中某 submodule 本地缺失
- **THEN** `git submodule add` 拉取，有 tag 则 checkout
- **WHEN** 本地目录不在 `.project.submodules` 中
- **THEN** 逐个询问后删除
- **WHEN** cmake 失败且 submodule 缺失 → 打印提示

---

## 架构变更检测

`build/` 下存放 `.build_arch` 标记文件，由 `build.sh` 内联逻辑管理。

- **WHEN** ARCH 值与上次构建不同 → 自动 `rm -rf build/` 后重新构建
- **WHEN** ARCH 值与上次构建相同 → 正常增量编译

---

## Git 忽略规则

- **WHEN** 执行 git commit
- **THEN** `.project.local.config` 和 `install/` 被 `.gitignore` 忽略