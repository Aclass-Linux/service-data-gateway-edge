## 1. 创建 egw_defs.h

- [x] 1.1 从 config.h 中提取 `egw_err_t` 枚举到 `src/core/egw_defs.h`，补充 include guard 和文件注释
- [x] 1.2 在 `egw_defs.h` 中添加 `EGW_CONF_REGISTER` 宏文档注释（宏定义放 config.h，这里只做引用说明）
- [x] 2.1 在 config.h 顶部添加 `#include "egw_defs.h"`
- [x] 2.2 删除 config.h 中的 `egw_err_t` 枚举定义
- [x] 2.3 调整 `EGW_CONF_MAX` 位置，保持逻辑顺序（错误码移除后重新对齐）
- [x] 2.4 添加 `EGW_CONF_REGISTER` 宏定义
- [x] 3.1 检查 config.c 中是否有直接依赖错误码枚举的部分，改为通过 egw_defs.h 引入
- [x] 3.2 将 `demo_module.c` 中手写的 `__attribute__((constructor))` 替换为 `EGW_CONF_REGISTER` 宏

## 4. 构建验证

- [x] 4.1 执行 build 命令验证无编译错误
- [x] 4.2 执行 `./build/bin/gateway -c config.json` 验证运行正常、输出一致
