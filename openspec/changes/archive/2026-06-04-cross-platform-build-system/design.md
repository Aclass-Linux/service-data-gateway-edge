## Context

当前构建系统仅支持 x86_64 本地编译。因项目需要部署到 ARM32/ARM64 目标板，且开发阶段需在 x86_64 上调试，必须在开发机上通过单一配置快速切换架构。

## Goals / Non-Goals

**Goals:**
- `.project.config` + `.project.local.config` 双层配置驱动架构、链接方式、优化等级
- build（Debug）和 release（Release）两条命令分离
- 产物统一输出到 `bin/` + `lib/`
- 切架构自动清理 build 目录，避免残留
- Git submodule 管理第三方依赖
- `$ORIGIN/../lib` 统一 rpath

**Non-Goals:**
- 不涉及部署脚本（rsync/打包/tarball，后续单独 change）
- 不实现 CI/CD
- 不编写交叉编译器安装脚本
- arm64、arm32 等其他架构工具链文件延后补充

## Decisions

### Decision 1: 双层配置（公共 + 本地覆盖）

拆为两个文件，加载顺序为 `.project.config` → `.project.local.config`，后者覆盖前者。

```bash
# .project.config（提交 git）
ARCH=x86_64            # x86_64 / arm64 / arm32
DGH_LINK=shared        # shared / static
CMAKE_BUILD_TYPE=Debug # Debug / Release / RelWithDebInfo / MinSizeRel
```

```bash
# .project.local.config（.gitignore，每人不同）
# 交叉编译时：
CROSS_COMPILE_PATH=/opt/toolchains/gcc-arm-9.2-2019.12-x86_64-arm-linux-gnueabihf/bin
SYSROOT_PATH=/opt/sysroots/armv7
```

第三方依赖通过 `third-party/` 下的 git submodule 管理，`CMakeLists.txt` 中 `add_subdirectory()` 引入。
命令行可临时覆盖：`ARCH=arm64 build`。
`release` 命令强制使用 `-DCMAKE_BUILD_TYPE=Release`，不受配置文件影响。

### Decision 2: Build/Release 优化等级分离

`build` 使用 `.project.config` 中 `CMAKE_BUILD_TYPE` 的值（默认 Debug）。
`release` 强制设为 Release，产生优化后的产物用于部署。
用户可通过环境变量临时覆盖：`CMAKE_BUILD_TYPE=MinSizeRel build`。

### Decision 3: 架构变更自动检测

`build/` 下存放 `.build_arch` 标记文件。build() 比较当前 ARCH 与标记值，不等则 `rm -rf build/` 后重新配置编译。

### Decision 4: 产物目录统一

```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
```

### Decision 5: rpath 统一

```cmake
set_target_properties(gateway PROPERTIES
    BUILD_RPATH  "\$ORIGIN/../lib"
    INSTALL_RPATH "\$ORIGIN/../lib"
)
```

### Decision 6: acclass.env.sh 调用 scripts/

`aclass.env.sh` 保持薄壳，具体逻辑委托给 `scripts/`。`scripts/build.sh` 保持独立可执行（可直接 `./scripts/build.sh` 运行）。

```
aclass.env.sh
  ├── build()     → source scripts/build.sh
  ├── release()   → source scripts/release.sh
  ├── clean()     → source scripts/clean.sh
  ├── run()       → 直接执行二进制
  ├── submodule-add <url> <path>   → source scripts/submodule.sh add <url> <path>
  ├── submodule-rm <path>          → source scripts/submodule.sh rm <path>
  └── submodule-sync               → source scripts/submodule.sh sync
```

### Decision 7: Toolchain 文件管编译器

`cmake/toolchain-{arch}.cmake` 定义各架构的编译器。x86_64 用系统默认编译器，armv7 用交叉编译器：

```cmake
# cmake/toolchain-x86_64.cmake
# 空文件或仅设 CMAKE_SYSTEM_NAME=Linux，使用系统默认编译器

# cmake/toolchain-armv7.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER ${CROSS_COMPILE_PATH}/arm-linux-gnueabihf-gcc)
set(CMAKE_SYSROOT ${SYSROOT_PATH})
```

`build()` 根据 ARCH 选择对应的 toolchain 文件传入 cmake。

### Decision 8: 自动初始化 local config

`aclass.env.sh` 被 source 时检查 `.project.local.config` 是否存在：
- 不存在 → 根据 `.project.config` 中的 ARCH 值自动生成模板文件，打印提示信息：
  ```
  Created .project.local.config from template.
  For cross-compilation add toolchain paths:
    CROSS_COMPILE_PATH=/path/to/your/toolchain/bin
    SYSROOT_PATH=/path/to/your/sysroot
  ```
- `build()` 在配置 cmake 前检查 submodule 是否已初始化和更新，缺失则自动执行 `git submodule update --init`
- 提供 `submodule-add` 和 `submodule-rm` 两条命令封装 submodule 操作

### Decision 9: ARM 架构下 run 报错

`run()` 检查 `ARCH` 值：
- `ARCH=x86_64` → 正常执行 `./build/bin/gateway`
- `ARCH=armv7` → 打印错误并退出：
  ```
  Cannot run ARM binary on this host.
  Deploy install/ to target board or set ARCH=x86_64 for local testing.
  ```

### Decision 10: .gitignore 规则

```
.gitignore 新增：
  .project.local.config   ← 个人工具链路径，不提交
  install/                ← Release 产物，cmake --install 输出目录，可重复生成
  build/                  ← Debug 产物，可重复生成（已在 .gitignore 中）
```

### Decision 11: Release 自动清理 install

`release()` 每次执行时先 `rm -rf install/`，再 `cmake --install`，确保 install 内容与当前 build 完全一致，无旧文件残留。

### Decision 12: Git Submodule 依赖管理

第三方依赖通过 `third-party/` 下的 git submodule 管理。每个依赖使用 tag 固定版本。

```bash
# 添加依赖
git submodule add https://github.com/eclipse/paho.mqtt.c.git third-party/paho-mqtt-c
cd third-party/paho-mqtt-c && git checkout v1.3.13 && cd ../..
git commit -m "add submodule: paho-mqtt-c v1.3.13"
```

`CMakeLists.txt` 中通过 `add_subdirectory()` 引入：

```cmake
add_subdirectory(third-party/paho-mqtt-c)
target_link_libraries(gateway PRIVATE paho-mqtt3)
```

`aclass.env.sh` 的 `build()` 在 cmake 配置前检查 submodule 是否已拉取：

```bash
# toolchain.sh 中
check_submodules() {
    if [ ! -f "${PROJECT_ROOT}/third-party/paho-mqtt-c/CMakeLists.txt" ]; then
        echo "Initializing submodules ..."
        git -C "${PROJECT_ROOT}" submodule update --init
    fi
}
```

跨架构时 submodule 源码不变，只需提供不同的 toolchain 文件指定编译器。

## Architecture

```
.project.config   ← 公共配置
.project.local.config ← 本地覆盖（.gitignore）
    │
    ▼
aclass.env.sh  ← 加载配置，提供 build/release/clean/run 命令
    │
    ├── build()     → scripts/build.sh     → cmake → build/bin/ + build/lib/
    ├── release()   → build + scripts/release.sh → install/bin/ + install/lib/
    ├── clean()     → scripts/clean.sh     → rm -rf build/
    └── run()       → ./build/bin/gateway

third-party/            build/                  install/
├── paho-mqtt-c/        ├── bin/gateway         ├── bin/gateway
└── ...                 ├── lib/*.so *.a        ├── lib/*.so *.a
                        ├── .build_arch         └── ...
                        └── CMakeCache.txt
```

## Risks / Trade-offs

- [submodule 版本] 依赖版本由 submodule 指向的 commit 锁定，更新需修改指向
- [仓库体积] third-party/ 增加约 8MB，clone 时加 `--recursive`
- [.project.config 未 gitignore] 需要提交仓库，每人本地可修改
- [ARM 本地运行] run 命令在 ARM 架构下无法本地执行 → 提示用户或调用 QEMU
