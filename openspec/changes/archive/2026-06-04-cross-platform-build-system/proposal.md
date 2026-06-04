## Why

项目处于 M1 阶段，当前构建系统仅支持 x86_64 本地编译。网关最终要部署到 ARM32/ARM64 目标板，需要在开发机上通过单一配置入口切换架构，实现一次开发、多平台发布。

## What Changes

- 新增 `.project.config`（公共）和 `.project.local.config`（本地覆盖）双层配置
- 重写 `aclass.env.sh`：加载双层配置，build/release 命令分离，release 包含 build + install
- 新增 `cmake/toolchain-x86_64.cmake` 和 `cmake/toolchain-armv7.cmake` 工具链文件
- 重构 CMakeLists.txt：统一输出目录、rpath，通过 git submodule 管理第三方依赖
- build/ 目录结构扁平化（bin/ + lib/）
- 切架构自动检测 `.build_arch`，自动清理重编
- 保留 `scripts/`，`aclass.env.sh` 调用其实现具体功能

## Capabilities

### New Capabilities
- `build-system-config`: 跨平台构建系统，通过 `.project.config` + `.project.local.config` 控制架构（x86_64/armv7）、链接方式（shared/static）、优化等级（Debug/Release）

## Impact

- `.project.config` — 新增，公共配置入口
- `.project.local.config` — 新增，个人本机配置（.gitignore）
- `.gitignore` — 添加 `.project.local.config`、`install/`
- `third-party/paho-mqtt-c` — 新增 git submodule，第三方依赖源码
- `cmake/toolchain-x86_64.cmake` — 新增，本地原生编译
- `cmake/toolchain-armv7.cmake` — 新增，交叉编译
- `aclass.env.sh` — 重写，新增 release 命令，架构检测
- `CMakeLists.txt` — 重构，输出目录、rpath、submodule 集成
- `scripts/` — 保留，新增 release.sh、toolchain.sh
- `src/app/CMakeLists.txt` — 适配新输出目录和 rpath
