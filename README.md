# DataGatewayHub

Linux 边缘网关，负责设备数据的采集和上报。

## 配置文件

### `.project.config` — 公共配置

项目构建参数，每项都有详细注释。所有变量通过 `build.sh` 自动传给 CMake，增删配置项只需改这个文件。

### `.project.local.config` — 个人本地配置（不提交 git）

```
COMPILE_PATH=/opt/toolchains/...
SYSROOT_PATH=/opt/sysroots/...
```

本地配置会被 `.project.config` 中的同名变量覆盖。

## 构建

```bash
source aclass.env.sh
build                  # Debug 编译
release                # Release 编译并安装到 install/
clean                  # 清理 build/
rebuild                # clean + build
run                    # 运行网关
```

## 目录结构

```
├── .project.config          公共构建配置
├── .project.local.config    个人本地配置（gitignored）
├── cmake/                   项目级 CMake 配置及工具链
├── scripts/                 构建脚本
├── src/
│   ├── core/                基础组件（错误码、事件循环、配置、总线、运行时）
│   ├── transport/           传输层（串口、TCP）
│   ├── protocol/            协议解析（Modbus RTU）
│   ├── ptable/              点表加载（mmap）
│   └── app/                 可执行入口
├── third-party/             第三方依赖（cjson, libuv, unity）
├── tests/                   单元测试（Unity）
├── build/                   编译产物
├── docs/design.md           设计决策记录
├── docs/implementation.md   实施记录
└── install/                 Release 产物
```
