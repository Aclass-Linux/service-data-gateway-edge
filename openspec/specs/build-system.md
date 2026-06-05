# Build System

## 配置系统

### 双层配置

| 文件 | 用途 | 是否提交 |
|---|---|---|
| `.project.config` | 公共配置：ARCH、EGW_LINK、CMAKE_BUILD_TYPE | 是 |
| `.project.local.config` | 本地覆盖：CROSS_COMPILE_PATH、SYSROOT_PATH | 否（.gitignore） |

#### 配置加载顺序

- **WHEN** `aclass.env.sh` 被 source
- **THEN** 先加载 `.project.config` 设置默认值
- **AND** 再加载 `.project.local.config`（若存在）
- **AND** 后者覆盖前者同名变量

#### 默认值

- **WHEN** `.project.config` 不存在
- **THEN** 使用硬编码默认值（ARCH=x86_64, EGW_LINK=shared, CMAKE_BUILD_TYPE=Debug）

#### 架构切换

- **WHEN** 修改 ARCH 值
- **THEN** 下次 `build` 时检测到架构变更
- **AND** 自动清理 build/ 目录后重新编译

---

## 构建命令

### Build

- **WHEN** 执行 `build`
- **THEN** cmake 配置 + 编译，产物输出到 `build/bin/` 和 `build/lib/`
- **AND** 设 BUILD_RPATH=$ORIGIN/../lib

### Release

- **WHEN** 执行 `release`
- **THEN** 先执行 build，再 cmake --install 到 `install/`
- **AND** 输出 `install/bin/gateway` 和 `install/lib/*.so`
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

| ARCH | 工具链文件 | 编译器 |
|------|-----------|--------|
| x86_64 | `cmake/toolchain-x86_64.cmake` | 系统默认 |
| armv7 | `cmake/toolchain-armv7.cmake` | COMPILE_PATH/arm-linux-gnueabihf-gcc |

#### 编译器路径检查

- **WHEN** `ARCH=armv7` 且 `COMPILE_PATH` 已配
- **THEN** 检查编译器是否存在，不存在时报错退出

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

- **WHEN** EGW_LINK=shared → 动态链接
- **WHEN** EGW_LINK=static → 静态链接

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

`build/` 下存放 `.build_arch` 标记文件。

- **WHEN** ARCH 值与上次构建不同 → 自动 `rm -rf build/` 后重新构建
- **WHEN** ARCH 值与上次构建相同 → 正常增量编译

---

## Git 忽略规则

- **WHEN** 执行 git commit
- **THEN** `.project.local.config` 和 `install/` 被 `.gitignore` 忽略