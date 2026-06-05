## Why

当前 gateway 是 hello world 测试桩，没有任何运行时配置系统。M1 后续需要 MQTT 连接、Modbus 设备采集、看门狗等模块，每加一个模块就需要一套配置解析。需要一个统一的、可复用的运行时配置框架，各模块自管配置，避免重复造轮子和耦合。

## What Changes

- 引入 JSON 作为运行时配置文件格式，cJSON 作为底层解析库
- 在 `core/` 层实现通用配置解析框架（注册、分发、取值封装）
- 采用自注册模式，各模块自己注册需要处理的配置 key
- 模块通过封装取值器（egw_conf_string/int/bool）解析配置，不直接操作 cJSON
- 所有模块配置存储在模块内部 static 变量中
- cJSON 源码直接放置于 third-party/cjson/，精简后只保留核心文件

## Capabilities

### New Capabilities
- `config-framework`: 运行时配置解析框架，包括注册、分发、取值封装

### Modified Capabilities

（空）

## Impact

- `src/core/` 下新增 `config.c` / `config.h`，实现框架核心逻辑
- 各模块（mqtt、modbus 等）各自实现 `*_parse()` 解析函数
- third-party/cjson/ 下放置 cJSON 精简源码（仅核心文件 + LICENSE + 版本信息）
- CMakeLists.txt 中 cJSON 以源码形式编译进 `egw_core` 库
- 新增 `config.json` 作为运行时配置文件
