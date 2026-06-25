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

## 测试

详见 [docs/testing.md](docs/testing.md)。

```bash
source aclass.env.sh
build
ctest --test-dir build --output-on-failure
./tools/virtual_serial.sh start
rm -f config.db && python3 tools/init_db.py config.db
run
./tools/virtual_serial.sh stop
```

## 目录结构

```
├── .project.config          公共构建配置
├── .project.local.config    个人本地配置（gitignored）
├── cmake/                   项目级 CMake 配置及工具链
├── scripts/                 构建脚本
├── src/
│   ├── core/                核心库：错误码、CRC、配置、总线、状态机
│   ├── transport/           传输层：串口/TCP 统一 handle 抽象（非阻塞 I/O）
│   ├── protocol/            协议层：帧定界（零拷贝 reserve/commit）+ Modbus RTU/TCP
│   ├── ptable/              点表加载（SQLite → 内存连续数组）
│   └── app/                 可执行入口（libuv 事件驱动 + Modbus 回环演示）
├── third-party/             第三方依赖（cjson, libuv, sqlite, unity）
├── tests/                   单元测试
├── tools/                   工具脚本（虚拟串口、数据库初始化）
├── build/                   编译产物
├── docs/
│   ├── design.md            设计决策记录（ADR）
│   ├── implementation.md    实施记录
│   └── testing.md           测试流程
├── CONTEXT.md               领域术语表
├── AGENTS.md                AI 编码助手指南
└── install/                 Release 产物
```

## 模块依赖

```
app → core, transport, protocol, ptable
protocol → core（不依赖 transport）
transport → core（不依赖 protocol）
ptable → core, sqlite
```

App 层编排 I/O，Protocol 纯解析，Transport 纯 I/O——三者解耦。
