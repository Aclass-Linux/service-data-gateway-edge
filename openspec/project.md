# DataGatewayHub — Project Overview

## 项目定位

Linux 边缘网关（主站），采集 Modbus、IEC 104、自定义协议数据，本地边缘计算后通过 MQTT、Modbus TCP 等协议供轮询/发布。

## 技术栈

| 项目 | 值 |
|---|---|
| 语言 | C（C11） |
| 构建 | CMake ≥ 3.20 |
| 目标平台 | Linux |
| 快捷开发 | `source aclass.env.sh` → `build`/`clean`/`run` |

## 当前阶段：M1 单进程原型

项目按里程碑渐进实现，详见 `项目计划.md`。当前为 M1（第 1-6 周）：

| 周 | 任务 | 产出 |
|---|---|---|
| 1 | 模拟数据源 FIFO + 信号处理 | simulator.c |
| 2 | 主控读取 FIFO + 时间戳 | gateway_v1.c |
| 3 | 守护进程 + syslog | gateway_v1 增强 |
| 4 | MQTT 上传（Paho） | 数据上云 |
| 5 | 看门狗 + 子进程管理 | 高可用 |
| 6 | 调试 + 稳定性 | 稳定版 |

## 架构

四层结构（详见 `openspec/specs/architecture.md`）：

```
app → protocol → connectors → core
```

各层实现状态详见架构文档。

## 关键文档

| 文件 | 内容 |
|---|---|
| `项目说明.md` | 12 个月完整学习路线图 |
| `项目计划.md` | M1-M7 每周详细任务 |
| `openspec/specs/architecture.md` | 架构定义与实现状态 |
| `openspec/specs/api-core.md` | Core API 清单 |
| `AGENTS.md` | OpenCode 快速上手指南 |
| `journal/` | 每日学习记录 |

## 目录结构

```
├── aclass.env.sh          # source 后获得快捷命令
├── scripts/               # 独立构建/清理/安装脚本
├── src/app/               # 当前唯一有代码的目录
├── src/{core,protocol}/   # 空目录（M2+ 预留）
├── openspec/              # 规范驱动工作流
├── journal/               # 每日学习记录
└── tests/                 # 空（尚未引入测试）
```
