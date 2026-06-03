# DataGatewayHub 架构

## 分层总览

| 层 | 职责 | 链接方向 | 实现状态 |
|---|---|---|---|
| `app` | 可执行入口 | 依赖以下各层 | 🟡 部分实现 |
| `protocol` | 协议解析（Modbus、IEC 104、自定义） | 被 app 依赖 | ⚪ 未实现 |
| `connectors` | 外部适配器（MQTT、Modbus TCP 等） | 被 protocol 和 app 依赖 | ⚪ 未实现 |
| `core` | 基础组件（日志、配置、错误码） | 被以上各层依赖 | ⚪ 未实现 |

> `hub`（编排与业务流）和 `data`（仓库与连接抽象）层待后续规划时补充。

## 各层详述

### app — 可执行入口

- **职责**：程序入口，组装各模块，管理生命周期
- **当前实现**：`src/app/main.c` — hello world 测试桩
- **M1 目标**：实现 `gateway_v1` 主控程序（FIFO 读取 → 日志 → MQTT 上传 → 看门狗）
- **依赖**：`protocol`、`connectors`、`core`

### protocol — 协议解析（未实现）

- **职责**：Modbus、IEC 104、自定义协议的报文解析与组包
- **当前实现**：无
- **M1 目标**：暂不涉及（M2 起逐步实现）
- **依赖**：`core`

### connectors — 外部适配器（未实现）

- **职责**：MQTT 发布/订阅、Modbus TCP 轮询、采集子进程管理等外部通信
- **当前实现**：无
- **M1 目标**：集成 Paho MQTT C 客户端库实现数据上传
- **依赖**：`core`

### core — 基础组件（未实现）

- **职责**：日志记录、配置管理、错误码定义、通用工具
- **当前实现**：无
- **M1 目标**：提供 `syslog` 封装、`--daemon` 守护进程支持
- **依赖**：无（被所有层依赖）

## 层间依赖关系

```
app
├── protocol → core
├── connectors → core
└── core
```

## M1 阶段范围

M1（第 1-6 周）仅涉及 `app` 层的实现。`protocol`、`connectors`、`core` 在本阶段提供目录结构但不提供实现代码。各层在 M2 起逐步填充。
