## Purpose

定义跨平台构建系统的双层配置驱动、build/release 分离、产物目录、架构检测、输出目录等规则。

## Requirements

### Requirement: 配置入口统一

项目 SHALL 通过 `.project.config` 作为公共构建配置入口，`.project.local.config` 作为个人本机配置覆盖。

#### Scenario: 配置加载顺序

- **WHEN** `aclass.env.sh` 被 source
- **THEN** 先加载 `.project.config` 设置默认值
- **AND** 再加载 `.project.local.config`（若存在）
- **AND** `.project.local.config` 中的值覆盖 `.project.config` 中的同名变量

#### Scenario: 默认值

- **WHEN** `.project.config` 不存在
- **THEN** 使用硬编码默认值（ARCH=x86_64, DGH_LINK=shared, CMAKE_BUILD_TYPE=Debug）

#### Scenario: 个人配置不提交

- **WHEN** 执行 git commit
- **THEN** `.project.local.config` 被 `.gitignore` 忽略
- **AND** 每个开发者可维护自己的工具链路径

#### Scenario: 架构切换

- **WHEN** 修改 `.project.config` 或 `.project.local.config` 中的 ARCH 值
- **THEN** 下次 `build` 时检测到架构变更
- **AND** 自动清理 build/ 目录后重新编译

### Requirement: Build/Release 分离

项目 SHALL 提供 `build` 和 `release` 两条命令。

#### Scenario: Build 命令

- **WHEN** 执行 `build`
- **THEN** cmake 配置 + 编译产物输出到 `build/bin/` 和 `build/lib/`
- **AND** 设 BUILD_RPATH=$ORIGIN/../lib

#### Scenario: Release 命令

- **WHEN** 执行 `release`
- **THEN** 先执行 build，再 cmake --install 到 `install/`
- **AND** install/ 下输出 `bin/gateway` 和 `lib/*.so`
- **AND** 设 INSTALL_RPATH=$ORIGIN/../lib

### Requirement: 产物目录统一

构建产物 SHALL 按类型输出到统一目录。

#### Scenario: 产物分类

- **WHEN** 编译完成
- **THEN** 可执行文件在 `build/bin/`
- **AND** 动态库(.so)和静态库(.a)在 `build/lib/`

### Requirement: 架构变更检测

构建系统 SHALL 记录上次构建的架构，变更时自动清理。

#### Scenario: 架构检测

- **WHEN** ARCH 值与上次构建不同
- **THEN** 自动执行 `rm -rf build/` 后重新构建
- **AND** `.build_arch` 更新为当前 ARCH 值

#### Scenario: 架构未变

- **WHEN** ARCH 值与上次构建相同
- **THEN** 正常增量编译，不清除 build/

### Requirement: 编译优化等级可配置

`.project.config` SHALL 支持 `CMAKE_BUILD_TYPE`，决定编译优化和调试符号。

#### Scenario: 默认 Debug

- **WHEN** CMAKE_BUILD_TYPE 未设置
- **THEN** 默认为 Debug（-O0 -g）

#### Scenario: Release 命令覆盖

- **WHEN** 执行 `release` 命令
- **THEN** 强制使用 CMAKE_BUILD_TYPE=Release（-O3 -DNDEBUG）
- **AND** 忽略 `.project.config` 中的设置

#### Scenario: 用户可覆盖

- **WHEN** 执行 `CMAKE_BUILD_TYPE=RelWithDebInfo build`
- **THEN** 使用命令行传入的值（-O2 -g -DNDEBUG）

### Requirement: 链接方式可配置

项目 SHALL 支持动态库（shared）和静态库（static）两种链接方式。

#### Scenario: 链接切换

- **WHEN** DGH_LINK=shared
- **THEN** 第三方库和项目库使用动态链接

- **WHEN** DGH_LINK=static
- **THEN** 第三方库和项目库使用静态链接

### Requirement: 独立可执行脚本

`scripts/build.sh` SHALL 支持独立运行，不依赖 `aclass.env.sh`。

#### Scenario: 直接调用

- **WHEN** 执行 `./scripts/build.sh`
- **THEN** 自动加载 `.project.config` 和 `.project.local.config`
- **AND** 行为与 `source aclass.env.sh && build` 一致

### Requirement: Local config 自动初始化

`aclass.env.sh` SHALL 在 source 时自动检查 local config，不存在则创建模板。

#### Scenario: 首次 source

- **WHEN** `aclass.env.sh` 被 source 且 `.project.local.config` 不存在
- **THEN** 根据当前 `.project.config` 的 ARCH 生成模板文件
- **AND** 打印提示信息指导用户填写工具链路径

#### Scenario: 编译器路径检查

- **WHEN** `ARCH=armv7` 且 `CROSS_COMPILE_PATH` 已配置
- **THEN** 检查 `$CROSS_COMPILE_PATH/arm-linux-gnueabihf-gcc` 是否存在
- **AND** 不存在时打印错误并退出，提示用户检查 `.project.local.config`

### Requirement: ARM 运行保护

跨架构编译时 SHALL 阻止在本地直接运行 ARM 二进制。

#### Scenario: ARM run 拦截

- **WHEN** `ARCH=armv7` 时执行 `run`
- **THEN** 打印错误信息，不执行二进制
- **AND** 提示用户部署到目标板或切回 x86_64

### Requirement: Git 忽略规则

项目 SHALL 在 `.gitignore` 中忽略不可提交的构建产物和个人配置。

#### Scenario: 忽略范围

- **WHEN** 执行 git commit
- **THEN** `.project.local.config` 被忽略
- **AND** `install/` 被忽略

### Requirement: Release 自动清理

`release` 命令 SHALL 在 install 前清理 install 目录，确保无旧文件残留。

#### Scenario: 每次 release 清理

- **WHEN** 执行 `release`
- **THEN** 先 `rm -rf install/`
- **AND** 再 `cmake --install`