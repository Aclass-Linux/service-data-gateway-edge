## Context

当前 `src/core/config.h` 承担了三件事：全局错误码定义、注册宏、配置框架 API。随着项目扩展，`egw_err_t` 应该被 `core` 之外的其他层（protocol、connectors）使用，但它们不需要配置框架。拆分为 `egw_defs.h` + `config.h` 后，各模块可以只包含自己需要的头文件。

## Goals / Non-Goals

**Goals:**
- `egw_err_t` 从配置框架中分离，放在 `egw_defs.h`
- `EGW_CONF_REGISTER` 宏放在 `config.h`（属于配置框架，但对外提供）
- `config.h` 通过 `#include "egw_defs.h"` 引用错误码
- `config.c` 改为包含 `egw_defs.h`
- `demo_module.c` 改用 `EGW_CONF_REGISTER` 宏

**Non-Goals:**
- 不改变任何 API 签名或行为
- 不修改 CMake 构建配置（不增删编译单元）

## Decisions

### 1. 文件拆分
```
egw_defs.h ← 全局错误码、编译器包装宏、通用类型别名
config.h   ← 配置框架 API + EGW_CONF_REGISTER 宏 + #include "egw_defs.h"
```

### 2. EGW_CONF_REGISTER 宏
```c
/* config.h */
/* GCC extension, not C11 */
#define EGW_CONF_REGISTER(key, handler) \
    static void __attribute__((constructor)) egw_reg_##handler(void) { \
        egw_conf_register((key), (handler)); \
    }
```

### 3. 错误码
`egw_err_t` 保持全集枚举形式，后续模块新增错误码追加到 `egw_err_t` 末尾。

## Risks / Trade-offs

- [注意] `egw_defs.h` 被多个模块包含，修改时需要确认所有引用方
- [考虑] 因为去掉了 `egw_err_t` 和 `EGW_CONF_MAX` 调整位置，git diff 较大但逻辑无变化
