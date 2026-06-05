## 1. 重写 config.h

- [x] 1.1 删除旧 API：`egw_conf_register()`、`EGW_CONF_REGISTER`、`EGW_CONF_MAX`、`egw_conf_handler_t`、`egw_conf_array_cb_t`
- [x] 1.2 删除旧函数：`egw_conf_string/int/bool/array/exists/raw`、`egw_conf_cleanup`
- [x] 1.3 新增 `egw_conf_t` 句柄类型（前向声明）
- [x] 1.4 新增 API 声明：`egw_conf_load()`、`egw_conf_free()`、`egw_conf_get_string/int/bool`、`egw_conf_array_length`
- [x] 1.5 保留 `#include "egw_defs.h"`
- [x] 2.1 删除注册表数组、`g_root`、`g_registry_count` 等旧静态变量
- [x] 2.2 实现 `egw_conf_load()`：读取文件 → cJSON_Parse → 分配句柄
- [x] 2.3 实现 `egw_conf_free()`：释放 cJSON 树和句柄
- [x] 2.4 实现 key path 解析器（点号分割 + `[n]` 下标）
- [x] 2.5 实现 `egw_conf_get_string()`：解析 key path → 取字符串值
- [x] 2.6 实现 `egw_conf_get_int()`：解析 key path → 取整数值
- [x] 2.7 实现 `egw_conf_get_bool()`：解析 key path → 取布尔值
- [x] 2.8 实现 `egw_conf_array_length()`：解析 key path → 返回数组长度
- [x] 3.1 改用 `egw_conf_load()` 获取句柄
- [x] 3.2 删除 demo_print_config() 调用，改为直接按新 API 打印
- [x] 3.3 调用 `egw_conf_free()` 退出前清理
- [x] 4.1 执行 build 命令验证无编译错误
- [x] 4.2 执行 `./build/bin/gateway -c config.json` 验证输出正确
