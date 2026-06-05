## Why

当前 `core/config.h` 混合了配置框架 API、全局错误码、handler 类型定义三件事。错误码 `egw_err_t` 应该是全项目共用的，不属于配置框架。`__attribute__((constructor))` 注册宏也需要一个统一的位置。拆分后 config.h 只做配置框架的事，公共定义独立出来。

## What Changes

- 新建 `src/core/egw_defs.h`：从 config.h 中提取 `egw_err_t` 错误码枚举、`EGW_CONF_REGISTER` 注册宏
- `config.h` 精简为纯配置框架 API，通过 `#include "egw_defs.h"` 引用错误码
- `config.c` 和 `demo_module.c` 更新 include 路径

## Capabilities

### New Capabilities
- `core-defs`: 全局公共定义（错误码、注册宏、属性包装）

### Modified Capabilities
- (空)

## Impact

- `src/core/config.h` 移除 `egw_err_t`、`EGW_CONF_MAX` 位置微调
- `src/core/config.c` 改为 `#include "egw_defs.h"` 而非直接依赖 config.h 中的错误码
- `src/app/demo_module.c` 使用 `EGW_CONF_REGISTER` 宏替代手写 `__attribute__((constructor))`
- 新建 `src/core/egw_defs.h`
