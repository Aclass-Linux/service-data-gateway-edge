# Simulator FIFO — 模拟数据源

## Purpose

M1 阶段（第 1 周）的模拟数据源模块。作为网关数据采集上游，生成随机温度值并通过 FIFO 输出。

## ADDED Requirements

### Requirement: 模拟数据源生成温度值

系统 SHALL 每秒生成一个随机温度值，范围 20-100（含），以浮点数表示。

#### Scenario: 正常生成

- **WHEN** simulator 运行
- **THEN** 每秒生成一个随机温度值
- **AND** 温度值在 20.0 到 100.0 之间（含边界）

#### Scenario: 生成值变化

- **WHEN** 连续两次读取生成值
- **THEN** 两次值不相同（随机性验证）

### Requirement: 模拟数据源通过模块化生成

温度生成逻辑 SHALL 封装在独立模块中，不直接写在主程序入口。

#### Scenario: 模块调用

- **WHEN** 主程序需要温度值
- **THEN** 调用 `data_gen.h` 提供的函数获取随机温度
- **AND** `data_gen.c` 为独立编译单元

### Requirement: 模拟数据源使用匿名管道传递数据

温度生成模块产出的数据 SHALL 通过匿名管道传递给写入模块，而非直接函数调用，以解耦数据生产与数据消费。

#### Scenario: 匿名管道传输

- **WHEN** 数据生成模块产生一个温度值
- **THEN** 该值通过匿名管道（pipe()）发送到数据消费端
- **AND** 消费端从管道读取端接收数据

### Requirement: 模拟数据源写入有名管道 FIFO

模拟数据源 SHALL 将温度值写入命名管道 `/tmp/temp_fifo`，供其他进程读取。

#### Scenario: FIFO 创建

- **WHEN** simulator 启动
- **THEN** 创建 FIFO 文件 `/tmp/temp_fifo`（若不存在）
- **AND** FIFO 权限为 0666

#### Scenario: FIFO 写入

- **WHEN** 温度值生成
- **THEN** 写入 `/tmp/temp_fifo`
- **AND** 每条记录为一行文本：`<温度值>\n`

### Requirement: SIGPIPE 信号处理

模拟数据源 SHALL 在 FIFO 无读端时处理 SIGPIPE 信号，防止进程因写入出错而崩溃。

#### Scenario: 无读端时写 FIFO

- **WHEN** FIFO 无读端打开
- **THEN** 写操作因 SIGPIPE 被处理
- **AND** 进程不崩溃，打印警告后继续运行
