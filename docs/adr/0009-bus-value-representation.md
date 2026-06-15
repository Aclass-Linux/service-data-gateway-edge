# ADR 0009: 总线值表示 —— union + 核心类型枚举

## 状态

已接受（枚举集合待最终确认，见 TODO）

## 背景

Pub/Sub 总线消息携带 `(device_id, sig_id, value)`。ADR-0005 定义"总线值 = 按 sig_id 约定的核心类型裸值，无内联类型标识"。ADR-0008 的持久化槽位是 `_Atomic uint64_t`（宽 8 字节，ARMv7 无锁原子要求）。二者需要共用同一种值的物理表示。

## 决策

总线值类型 `egw_value_t` 定义为无判别式 union：

```c
typedef union {
    uint8_t  b;       /* EGW_CTYPE_BOOL  — 线圈/离散量 */
    int16_t  i16;     /* EGW_CTYPE_I16 */
    uint16_t u16;     /* EGW_CTYPE_U16 */
    int32_t  i32;     /* EGW_CTYPE_I32 */
    uint32_t u32;     /* EGW_CTYPE_U32 */
    int64_t  i64;     /* EGW_CTYPE_I64 */
    uint64_t u64;     /* EGW_CTYPE_U64 */
    float    f32;     /* EGW_CTYPE_F32 */
    /* TODO: 是否保留 f64 成员待定（见下方 TODO-2） */
    uint64_t raw;     /* seqlock / 原子加载的位模式视图 */
} egw_value_t;        /* sizeof == 8，强制断言 */
static_assert(sizeof(egw_value_t) == 8, "egw_value_t must be 8 bytes");
```

核心类型枚举通过 X-macro 表定义（与 `egw_err.inc` 风格一致），文件暂定 `src/core/include/egw_ctype.inc`：

```c
/* EGW_CORE_TYPE(name, description) */
EGW_CORE_TYPE(BOOL, "boolean / coil")
EGW_CORE_TYPE(I16,  "int16")
EGW_CORE_TYPE(U16,  "uint16")
EGW_CORE_TYPE(I32,  "int32")
EGW_CORE_TYPE(U32,  "uint32")
EGW_CORE_TYPE(I64,  "int64")
EGW_CORE_TYPE(U64,  "uint64")
EGW_CORE_TYPE(F32,  "float32")
/* TODO-2: F64 是否加入（见下方） */
```

`raw` 成员是设计约定：持久化线程和 seqlock 只通过 `raw`（uint64_t）做原子加载/存储；业务侧按路由表中的核心类型枚举选具体成员解释。union 不带 tag，类型语义完全由路由表的 `egw_ctype_t` 字段承载。

## TODO（待确认）

**TODO-1：枚举初始集合是否有增删？**
I64/U64 在 Modbus 场景不常见，如果第一版不需要可先不加；需要时追加一行即可。

**TODO-2：是否保留 F64 成员？**
Union 已是 8 字节，加 `double f64` 零成本。之前决定"不强制所有值统一成 double"，但"union 里留一个 f64 成员备用"不冲突。可以只加成员不加枚举，等真有需求再加枚举项。

## 约束

- **union 宽度上限 8 字节**：ADR-0008 的 `_Atomic uint64_t` 要求 ≤8 字节，ARMv7 64 位原子才能无锁。任何超过 8 字节的值（字符串、字节数组）不进总线 union，走引用或 SQLite。
- **枚举值预留区间**：按项目规范，枚举项之间留编号间隙，方便后续插入不破坏 ABI。

## 否决的方案

**统一 double（原方案 A）**：所有值经 scale/offset 转换后统一成 double 发布。简单但丢失整型精度（虽然 double 可无损表示 53 位整数，但语义模糊）；且已决定 scale/offset 归各侧点表，总线值是转换后的核心类型，不一定经过 scale。

**tagged union（带判别式）**：消息自带 type tag，消费者无需查路由表。消息结构变宽，消费者代码多一层分支；且已决定类型语义由路由表统一承载，tag 重复了路由表的角色。

## 理由

- **与 ADR-0008 对齐**：8 字节 union + `raw` 视图直接塞进 `_Atomic uint64_t` 槽位，seqlock 和业务共用同一份内存，零转换。
- **与 ADR-0005 对齐**："无类型标识"的约定让总线消息最小，类型语义集中在路由表，不分散。
- **X-macro 扩展**：追加类型只改 `.inc` 一行，枚举和字符串函数自动生成，不手动同步。
- **无判别式 union**：C11 合法，`raw` 成员让原子操作有明确的类型视图，不需要 `memcpy` / `type-pun` 绕过严格别名。
