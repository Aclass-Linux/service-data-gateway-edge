## Context

当前 gateway 没有任何运行时配置系统。M1 需要支持 MQTT 连接、Modbus 采集、看门狗等模块，每个模块需要从配置文件读取自身参数。需要一个通用的配置框架，各模块只关心自己的配置解析，框架处理文件读取、Key 分发、取值封装。

## Goals / Non-Goals

**Goals:**
- 核心层提供 `core/config.c` 配置框架：文件加载、模块注册、key 分发、取值封装
- 各模块以自注册模式挂载到框架上，模块实现 `*_parse(egw_conf_t *conf)` 函数
- 取值封装函数（egw_conf_string/int/bool）屏蔽底层 cJSON API
- 每个模块的配置存储在模块内部 static 变量中，通过 getter 暴露给外部
- cJSON 源码直接放置于 third-party/cjson/，精简后保留核心文件

**Non-Goals:**
- 不实现配置文件热重载的运行时信号监听（模块 parse 需支持重复调用，但由外部触发）
- 不做跨模块配置校验（各模块自检，外部按需调用 validate）
- 不实现配置文件写回/修改
- 不实现 YAML/TOML 等其他配置格式

## Decisions

### 1. 配置格式：JSON

运行时配置使用 JSON 格式。cJSON 作为底层解析库，通过封装层隔离。

### 2. 注册方式：自注册

每个模块通过 `__attribute__((constructor))` 在编译期自动注册，不依赖 main() 手动调用。

```c
/* GCC extension, not C11 */
static void __attribute__((constructor)) egw_reg_mqtt(void) {
    egw_conf_register("mqtt", mqtt_parse);
}
```

框架内部使用固定数组存储注册表，模块数量可预期（M1 ≤ 4，M2+ ≤ 10）。

```c
#define EGW_CONF_MAX 16

static struct {
    const char *key;
    egw_conf_handler_t handler;
} g_registry[EGW_CONF_MAX];

static int g_registry_count = 0;
```

egw_conf_register() 满时返回错误。配置加载时遍历顶层 key，匹配注册表分发。

### 3. Handler 签名：封装取值器

```c
// 框架传给模块的封装类型
typedef struct egw_conf egw_conf_t;  // 内部包裹 cJSON*

// 模块的 parse 函数签名
int mqtt_parse(egw_conf_t *conf);

// 模块内部使用取值封装
g_cfg.broker = egw_conf_string(conf, "broker");
g_cfg.port   = egw_conf_int(conf, "port", 1883);
```

模块不直接操作 cJSON，通过封装函数取值。封装函数负责 NULL 检查、类型判断、默认值。

### 4. 多级分发：框架只派顶层

框架只做一次 key 匹配分发（`"mqtt"` 传给 `mqtt_parse`）。模块内部需要遍历子结构（如 serial_ports → devices）时，由模块自己调用 `egw_conf_array()` 等封装函数控制遍历逻辑。不同模块遍历模式不同，不适合统一抽象。

### 5. 配置存储：模块内部 static 变量

```c
// modbus/config.c
static struct modbus_cfg g_cfg;

const struct modbus_cfg *modbus_get_config(void) {
    return &g_cfg;
}
```

每个模块的配置在模块自己的 .c 文件中以 static 变量持有，对外暴露 const getter。不引入全局配置池，避免 void* 类型安全和测试隔离问题。

### 6. 错误处理

- 遇到未知 key 忽略，不报错
- 缺失必需 key 返回错误码，由调用者决定 exit 还是继续
- `egw_conf_string()` 等取值函数对缺失字段返回 NULL/默认值，不自行退出
- 一个模块 parse 失败不影响其他模块继续解析，egw_conf_load() 返回最后一次错误码

### 7. 模块重载支持

parse 函数需要能重复调用。每次调用前清空旧配置状态，重新填充。模块内部通过 egw_conf 取新值覆盖 static 变量。

### 8. 配置路径

框架通过形参接受配置文件路径。路径来源由调用者决定（命令行、环境变量、硬编码）。

### 9. 自动注册

各模块通过 `__attribute__((constructor))` 自动注册，不依赖 main() 中手动调用。每个模块的 .c 文件中定义 static 初始化函数。

```c
// 编译期自动注册，模块被编译即生效
/* GCC extension, not C11 */
static void __attribute__((constructor)) egw_reg_mqtt(void) {
    egw_conf_register("mqtt", mqtt_parse);
}
```

模块通过编译参与决定是否被注册，通过链接裁剪决定是否包含。

### 10. cJSON 引入方式

cJSON 源码直接放置于 `third-party/cjson/`，精简目录只保留核心文件（cJSON.c/h、cJSON_Utils.c/h、LICENSE），删除测试、CI、文档等无关文件。后续更新上游时手动 diff 后 cherry-pick。

`third-party/cjson/CMakeLists.txt` 中定义 option 和源文件变量，不定义 target，不编译。

```cmake
# ┌─────────────────────────────────────────────────────┐
# │ third-party/cjson/CMakeLists.txt                    │
# │ 职责：定义 option 控制功能开关，暴露源文件路径变量      │
# │ 注意：只设变量，不定义 target，不编译                  │
# └─────────────────────────────────────────────────────┘
option(EGW_CJSON_UTILS "Enable cJSON_Utils" OFF)
option(EGW_CJSON_PRINT "Enable cJSON print functions" ON)

set(EGW_CJSON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/cJSON.c)
set(EGW_CJSON_INC ${CMAKE_CURRENT_SOURCE_DIR})

if(EGW_CJSON_UTILS)
    list(APPEND EGW_CJSON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/cJSON_Utils.c)
endif()
```

```cmake
# ┌─────────────────────────────────────────────────────┐
# │ src/core/CMakeLists.txt                             │
# │ 职责：加载 cJSON 变量，定义 egw_core target，传播宏   │
# │ 注意：以下 4 行顺序不可颠倒                          │
# └─────────────────────────────────────────────────────┘
# ① 先加载 cJSON 模块，取得变量（EGW_CJSON_SRC / EGW_CJSON_INC）
add_subdirectory(${CMAKE_SOURCE_DIR}/third-party/cjson)

# ② 定义 egw_core target，将 cJSON 源文件加入编译
add_library(egw_core
    ${EGW_CJSON_SRC}
    config.c config.h
)

# ③ 添加 cJSON 头文件搜索路径（PRIVATE，不暴露给外部）
target_include_directories(egw_core PRIVATE ${EGW_CJSON_INC})

# ④ 设置宏定义（PRIVATE，仅 egw_core 编译时生效）
target_compile_definitions(egw_core PRIVATE
    $<$<BOOL:${EGW_CJSON_UTILS}>:EGW_HAS_CJSON_UTILS>
    $<$<NOT:$<BOOL:${EGW_CJSON_PRINT}>>:CJSON_PRINT=0>
)
```

宏命名按前缀分类：`CJSON_*` 为 cJSON 原生宏，`EGW_*` 为项目控制宏，混用无误。

`target_compile_definitions` 使用 `PRIVATE` 传播，cJSON 完全封装在 egw_core 内部，外部模块只能通过项目自己的 `egw_conf_*` API 访问配置。

### 11. egw_conf_t 结构体

```c
typedef struct egw_conf {
    cJSON *section;       /* 当前配置段的 cJSON 对象 */
    const char *error;    /* 可选错误信息 */
} egw_conf_t;
```

轻量 wrapper，只包裹 cJSON* 和错误信息，不做厚重抽象。

### 12. 注册表实现

```c
#define EGW_CONF_MAX 16

static struct {
    const char *key;
    egw_conf_handler_t handler;
} g_registry[EGW_CONF_MAX];

static int g_registry_count = 0;
```

固定数组，无动态内存问题。满时返回错误。

### 13. egw_conf_array 回调签名

```c
typedef int (*egw_conf_array_cb)(egw_conf_t *item, void *ctx);

int egw_conf_array(egw_conf_t *conf, const char *key,
                   egw_conf_array_cb cb, void *ctx);
```

- item：每个数组元素的 wrapper
- ctx：透传用户上下文（如 struct modbus_cfg*）
- 回调返回非 0 中止遍历

### 14. 清理策略

```c
int egw_conf_load(const char *path) {
    egw_conf_cleanup();    /* 每次加载前自动清理旧树 */
    /* ... 解析 json → 分发 ... */
}

void egw_conf_cleanup(void) {
    if (g_root) cJSON_Delete(g_root);
    g_root = NULL;
}
```

自动支持重载——反复调 egw_conf_load() 不会有内存泄漏。

### 15. 配置文件路径

通过 `-c` 参数传入，默认 `config.json`。

```c
const char *cfg_path = "config.json";
int opt;
while ((opt = getopt(argc, argv, "c:")) != -1) {
    switch (opt) {
        case 'c': cfg_path = optarg; break;
        default: return 1;
    }
}
if (egw_conf_load(cfg_path) != EGW_OK) return 1;
```

## Risks / Trade-offs

- [风险] 自注册顺序不确定性 → 各模块间无依赖关系，顺序无关
- [风险] 封装层限制灵活性 → 封装函数提供 escape hatch：`egw_conf_raw()` 返回裸 cJSON*
- [风险] 模块 parse 重复调用后静态变量覆盖 → 模块必须实现幂等 parse（先清零再填充）
- [考虑] cJSON 以源码形式合入 egw_core → 无额外动态库依赖，换库时仅修改 core 的 CMake 和 third-party 目录
- [注意] third-party/cjson/ 已精简，更新上游时手动 diff 后 cherry-pick，不可同步整个仓库
