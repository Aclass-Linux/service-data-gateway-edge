# 构建脚本设计 — env.sh

## 血祭哲学

> 先让它在血里站起来，再考虑好不好看。

构建系统的终极目标是：**让开发者用最少的按键从零跑到输出**。为了这个目标，可以牺牲一切形式上的优雅。

## 为什么是 shell？

| 方案 | 代价 |
|------|------|
| Python 脚本 | 引入 `python3` 依赖；参数解析未必比 env var 方便 |
| Makefile | 隐藏状态难设；跨编译场景不如 env var 灵活 |
| CMake Presets | JSON 配置啰嗦；不支持运行时逻辑 |
| env.sh **( chosen )** | 零依赖，source 后命令常驻 shell |

env.sh 的核心理念是 **「source 一次，之后只敲动词」**：

```zsh
source scripts/env.sh
build     # 编译
run       # 运行
test      # 测试
clean     # 清理
```

无需路径、无需参数、无需记忆 cmake 命令长什么样。

## 配置优先级链

```
环境变量 > .project-config > 硬编码默认值
```

实现靠 shell 的 `:=` 空位补缺语义：

```bash
: "${ARCH:=x86}"     # 只有未设置/空时才赋值
```

### 实际效果

| 情况 | ARCH 最终值 | 赢家 |
|------|------------|------|
| `export ARCH=arm32` 在先，source 在后 | `arm32` | 环境变量 |
| 没设环境变量，.project-config 写了 `arm32` | `arm32` | 配置文件 |
| 都没设 | `x86` | 默认值 |
| source 后临时 `ARCH=arm64 build` | `arm64` | 单次覆盖 |

环境变量永远赢，因为它在 shell 启动时就已占位，配置文件用的 `:=` 无法覆盖已有值。

## 命令参考

| 命令 | 作用 | 透传参数 |
|------|------|---------|
| `build` | 编译 | 追加到 `cmake --build`（如 `-j8`） |
| `clean` | 删 build 目录 | — |
| `rebuild` | clean + build | 同 build |
| `run` | 运行 binary（自动先 build） | 传给 binary |
| `test` | 编译 + ctest | 追加到 `ctest`（如 `--test-dir ...`） |
| `sync` | git pull | — |

## 跨编译

靠 ARCH 环境变量切换：

```zsh
ARCH=arm32 build
```

env.sh 内部判断 `$ARCH` 为 `arm32` 时自动加载 `cmake/arm32-toolchain.cmake`。

## 扩展指南

### 增加 cmake 参数

编辑 `.project-config`：

```bash
: "${CMAKE_ARGS:=-DACLASS_BUILD_TESTS=OFF -DSOME_FEATURE=ON}"
```

或者在调用时：

```bash
CMAKE_ARGS="-DFOO=ON" build
```

### 增加新命令

在 `env.sh` 里加一个 shell 函数即可：

```bash
deploy() {
  rsync -avz "${PROJECT_ROOT}/out/bin/" user@target:/usr/local/bin/
}
```

source 后立刻可用。
