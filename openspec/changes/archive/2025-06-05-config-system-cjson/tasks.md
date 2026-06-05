## 1. CMake 集成

- [x] 1.1 精简 third-party/cjson/：删除 tests/、fuzzing/、.github/ 等非必要文件，只保留 cJSON.c/h、cJSON_Utils.c/h、LICENSE；创建 CMakeLists.txt 定义 EGW_CJSON_SRC 和 EGW_CJSON_INC 变量；补充 README.md 记录上游版本号
- [x] 1.2 创建 src/core/CMakeLists.txt，通过 add_subdirectory 引用 third-party/cjson，将 ${EGW_CJSON_SRC} 加入 egw_core 源文件列表，添加头文件路径
- [x] 1.3 更新根 CMakeLists.txt，添加 add_subdirectory(src/core)
- [x] 1.4 验证编译：gateway 链接 libegw_core 后能正常调用 cJSON API

## 2. 配置框架核心

- [x] 2.1 定义 egw_conf_t 结构体（包裹 cJSON*），实现 egw_conf_init() / egw_conf_cleanup()
- [x] 2.2 实现注册表：egw_conf_register() 将 key + handler 存入内部结构体数组
- [x] 2.3 实现 egw_conf_load()：读取 JSON 文件 → cJSON_Parse → 遍历顶层 key → 匹配注册表分发
- [x] 2.4 实现 egw_conf_string(conf, key)：返回字符串值，缺失返回 NULL
- [x] 2.5 实现 egw_conf_int(conf, key, def)：返回整数值，缺失返回默认值
- [x] 2.6 实现 egw_conf_bool(conf, key, def)：返回布尔值，缺失返回默认值
- [x] 2.7 实现 egw_conf_array(conf, key, callback, ctx)：遍历 JSON 数组回调每个元素
- [x] 2.8 实现 egw_conf_exists(conf, key)：检查 key 是否存在
- [x] 2.9 实现 egw_conf_raw(conf, key)：返回裸 cJSON*，作为 escape hatch
- [x] 2.10 定义统一错误码：EGW_ERR_FILE_NOT_FOUND、EGW_ERR_PARSE、EGW_ERR_MISSING_KEY

## 3. 打印验证

- [x] 3.1 在 main() 中通过命令行参数 `-c config.json` 接受配置文件路径
- [x] 3.2 main() 加载配置后，按以下格式逐段打印各模块的配置内容：
- [x] 3.3 输出内容与 config.json 对应字段一致，才算验证通过

## 4. 构建与验证

- [x] 4.1 执行 build 命令验证无编译错误
- [x] 4.2 执行 `./build/bin/gateway -c config.json` 验证输出格式正确
- [x] 4.3 验证未知 key 被忽略（添加一个未注册的 key，输出不应出现该字段）
- [x] 4.4 验证缺失必填 key 返回错误码
