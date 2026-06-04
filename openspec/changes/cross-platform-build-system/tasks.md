## 1. 配置文件

- [x] 1.1 创建 `.project.config`（ARCH=x86_64, DGH_LINK=shared, CMAKE_BUILD_TYPE=Debug）
- [x] 1.2 创建 `.project.local.config` 模板（CROSS_COMPILE_PATH + SYSROOT_PATH）
- [x] 1.3 `.gitignore` 添加 `.project.local.config`、`install/`

## 2. 构建脚本

- [x] 2.1 重写 `aclass.env.sh`：source 时自动加载 `.project.config` + `.project.local.config`
- [x] 2.2 source 时检查 local config 是否存在，不存在则创建模板并打印提示
- [x] 2.3 实现 `build()`：.build_arch 检测 + cmake 配置编译 + 写标记 + 工具链路径检查
- [x] 2.4 实现 `release()`：build + cmake --install install
- [x] 2.5 实现 `clean()`、`rebuild()`
- [x] 2.6 实现 `run()`：ARCH=x86_64 正常运行，ARCH=armv7 报错提示
- [x] 2.7 实现 `submodule-add()`、`submodule-rm()`、`submodule-sync()`：调用 `scripts/submodule.sh`

## 3. Submodule

- [x] 3.1 添加 submodule：`git submodule add https://github.com/eclipse/paho.mqtt.c.git third-party/paho-mqtt-c`
- [x] 3.2 锁定版本：在 submodule 目录切到 v1.3.13 并提交

## 4. 工具链文件

- [x] 4.1 创建 `cmake/toolchain-x86_64.cmake`（使用系统默认编译器）
- [x] 4.2 创建 `cmake/toolchain-armv7.cmake`（引用环境变量 CROSS_COMPILE_PATH / SYSROOT_PATH）

## 5. CMakeLists 重构

- [x] 5.1 重构顶层 `CMakeLists.txt`：输出目录、rpath、submodule 集成
- [x] 5.2 重构 `src/app/CMakeLists.txt`：适配新输出结构、添加 DGH_LINK 控制

## 6. 重构 scripts/

- [x] 6.1 重写 `scripts/build.sh` 支持独立运行（自加载配置）
- [x] 6.2 新增 `scripts/release.sh`：cmake --install 逻辑
- [x] 6.3 新增 `scripts/toolchain.sh`：.build_arch 检测 + 架构变更清理 + submodule 检查
- [x] 6.4 新增 `scripts/submodule.sh`：submodule add / rm / sync 命令实现
- [x] 6.5 更新 `scripts/clean.sh` 适配新目录结构

## 7. 文档

- [x] 7.1 更新 `AGENTS.md`：构建命令和目录结构

## 8. 验证

- [x] 8.1 build 编译通过，`./build/bin/gateway` 正常运行
- [x] 8.2 release 产出 install/bin/gateway + install/lib/
- [x] 8.3 修改 .project.config ARCH，build 自动清理重编
- [x] 8.4 修改 .project.config DGH_LINK，build 使用对应链接方式
