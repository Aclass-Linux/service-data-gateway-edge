# Core Defs

全局公共定义：错误码、编译器包装宏。

## Requirements

### Requirement: 全局错误码定义

系统 SHALL 在 `egw_defs.h` 中定义 `egw_err_t` 枚举，供所有模块共享使用。

#### Scenario: 错误码使用
- **WHEN** 任一模块需要返回错误状态
- **THEN** 使用 `egw_err_t` 枚举中的错误码
- **AND** 新增错误码追加到枚举末尾

#### Scenario: 错误码覆盖范围
- **WHEN** 模块发生文件、解析、内存等通用错误
- **THEN** 使用 `egw_err_t` 中的预定义错误码，不自行定义新枚举

### Requirement: 注册宏

系统 SHALL 在 `config.h` 中提供 `EGW_CONF_REGISTER` 宏，简化模块配置注册。

#### Scenario: 宏调用
- **WHEN** 模块调用 `EGW_CONF_REGISTER("mqtt", mqtt_parse)`
- **THEN** 自动生成 `__attribute__((constructor))` 函数
- **AND** 函数内调用 `egw_conf_register()` 完成注册

### Requirement: 头文件包含关系

`egw_defs.h` SHALL 不依赖任何项目内部头文件。`config.h` SHALL 通过 `#include "egw_defs.h"` 引用错误码类型。

#### Scenario: 模块按需引用
- **WHEN** 模块只需要错误码
- **THEN** 包含 `egw_defs.h` 即可，不需要包含 `config.h`
- **WHEN** 模块需要配置框架
- **THEN** 包含 `config.h`，错误码自动可用
