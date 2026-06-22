# SQLite3 学习笔记

## sqlite3_open_v2

### 签名

```c
int sqlite3_open_v2(
    const char *filename,    /* 数据库文件路径（UTF-8） */
    sqlite3 **ppDb,          /* OUT：数据库连接句柄 */
    int flags,               /* 打开模式标志 */
    const char *zVfs         /* VFS 模块名（NULL 为默认） */
);
```

### flags 值

```c
SQLITE_OPEN_READONLY        // 只读打开
SQLITE_OPEN_READWRITE       // 读写打开
SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE  // 读写打开，不存在则创建
SQLITE_OPEN_URI             // 文件名解析为 URI
SQLITE_OPEN_MEMORY          // 纯内存数据库
SQLITE_OPEN_NOFOLLOW        // 拒绝符号链接
```

### 返回值

| 返回值 | 含义 |
|--------|------|
| `SQLITE_OK` (0) | 成功 |
| `SQLITE_CANTOPEN` | 文件无法打开 |
| `SQLITE_MISUSE` | 参数错误 |
| `SQLITE_NOMEM` | 内存不足 |

**成功不等于数据库文件存在**——如果 `SQLITE_OPEN_CREATE` 没设，即使文件不存在也返回 `SQLITE_OK`，但后续 `SELECT` 会失败。

### 项目中的用法

```c
/* 打开（不存在则创建） */
sqlite3 *db = NULL;
int rc = sqlite3_open_v2(db_path, &db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                         NULL);
if (rc != SQLITE_OK) { /* 失败 */ }

/* 关闭 */
sqlite3_close(db);
```

### 备注

- 返回的 `sqlite3 *` 是一个不透明句柄，所有后续 SQLite 操作都需要它
- 多个连接打开同一个 `.db` 文件会走 SQLite 的**文件级锁**（并发控制）
- `zVfs` 传 `NULL` 用默认 VFS（`os_unix` for Linux, `os_win` for Windows）
- 即使 `rc == SQLITE_OK`，**也可能在关闭时才对真正的文件 I/O 失败报错**（延迟语义）

### SQLITE_OPEN_URI 说明

不设此标志时，`"data.db?mode=ro"` 被当作**字面文件名**（磁盘上真找这个文件）。
设此标志后，文件名按 URI 解析，`?` 后跟参数：

```
file:data.db?mode=memory&cache=private
file:/path/to/data.db?vfs=unix
data.db?mode=ro
```

URI 参数：

| 参数 | 值 | 效果 |
|------|----|------|
| `mode` | `ro` / `rw` / `rwc` / `memory` | 覆盖 `sqlite3_open_v2` 的 flags |
| `cache` | `shared` / `private` | 是否共享缓存 |
| `vfs` | `unix` / `win32` / 自定义名 | 选择 VFS |

项目未启用 `SQLITE_OPEN_URI`，直接用普通文件路径。

### 第4参数：VFS（Virtual File System）

```c
sqlite3_open_v2(path, &db, flags, zVfs);
```

VFS 是 SQLite 的**文件 I/O 抽象层**，所有磁盘读写最终都通过它。`zVfs = NULL` 用**默认 VFS**（Linux 上用 `os_unix`，Windows 用 `os_win`）。

常见内置 VFS：
- `"unix"` — POSIX 文件系统（Linux/macOS）
- `"unix-none"` — 不加锁版
- `"unix-dotfile"` — 用 `.db-journal` 文件做锁（NFS 友好）
- `"win32"` — Windows 文件系统
- `"memdb"` — 纯内存（等价 `SQLITE_OPEN_MEMORY`）

自定义 VFS 可以替换整个文件 I/O 层（写加密、写到 flash 特殊分区、写到网络等）。

### 对比三种 open 函数

```c
int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_open16(const void *filename, sqlite3 **ppDb);  /* UTF-16 */
int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs);
```

| | `open` | `open16` | `open_v2` |
|---|---|---|---|
| 编码 | UTF-8 | UTF-16 | UTF-8 |
| 控制 flags | ❌ 固定 `READWRITE \| CREATE` | 同左 | ✅ 自定义 |
| 自定义 VFS | ❌ 默认 | ❌ 默认 | ✅ |
| URI 解析 | ❌ | ❌ | ✅ 加 `SQLITE_OPEN_URI` |

**改进点**：`open_v2` 相比 `open`：
1. **显式控制 flags**——可以选择只读不写、纯内存，不再隐含自动创建
2. **VFS 可替换**——测试环境切 `memdb`、实际跑切 `unix`，无需改代码
3. **URI 支持**——通过 URI 参数注入配置，灵活部署

### SQLite 文件级锁详解

SQLite 用**五级锁**实现并发控制。锁在**数据库连接**级别持有，不是行级。
**所有锁都是 SQLite 内部自动管理的**——你执行 `SELECT` / `UPDATE` 时，引擎自己决定升到哪一级，不需要手动 `LOCK TABLE`。

#### 锁状态机

```
UNLOCKED → SHARED → RESERVED → PENDING → EXCLUSIVE
                                              ↑
     多个读者←→写者可以同时执行
```

#### 各锁级别详解

| 状态 | 谁自动持有 | 能干什么 | 与其他锁兼容 |
|------|-----------|---------|------------|
| **UNLOCKED** | 刚 `open()` 或刚 `COMMIT` 完 | 什么都不干 | 任意 |
| **SHARED** | 执行 `SELECT` 时自动获取 | 只读数据库 | 允许多个 SHARED 并存 |
| **RESERVED** | `INSERT/UPDATE/DELETE` 第一条时自动获取 | 准备写（修改还在内存缓冲区） | SHARED ✅ / 其他 RESERVED ❌ |
| **PENDING** | 写者准备 `COMMIT` 时自动升级 | 等所有读者离开，然后独占 | 新 SHARED ❌ / 旧 SHARED 等它们走完 |
| **EXCLUSIVE** | `COMMIT` 落盘时自动升级 | 真正写入磁盘文件 | 全部 ❌ |

**关键规则**：
- **SHARED 不阻塞 SHARED**：100 个连接同时 SELECT 没问题
- **RESERVED 不阻塞 SHARED**：写者在缓冲区修改数据时，别人还能读
- **PENDING 阻塞新 SHARED**：写者要落盘了，不再接受新读者，等老读者走完
- **PENDING → EXCLUSIVE**：所有读者释放后升级，开始写磁盘

#### 锁什么时候被持有/释放？

```
SELECT：
  sqlite3_step() → SHARED 锁（自动持有）
  sqlite3_reset() / sqlite3_finalize() → 有些人会释放，有些人会保留
  有保留 SHARED 直到下一个 sqlite3_step() 或事务结束

UPDATE：
  sqlite3_step(UPDATE) → RESERVED 锁（自动持有）
  提交 COMMIT → PENDING → EXCLUSIVE → 写盘 → UNLOCKED（自动释放）

ROLLBACK → 自动回退到 UNLOCKED
```

#### 锁冲突时：sqlite3_busy_timeout

```c
int sqlite3_busy_timeout(sqlite3 *db, int ms);
```

当你的操作试图获取一个锁，但被另一个连接持有时：

```
sqlite3_step(SELECT)
  → 需要 SHARED 锁
    ├─ 无人持 EXCLUSIVE → 直接拿到，正常执行
    └─ 有人持 EXCLUSIVE（写者正在写盘）→ 无法获取
         ├─ 无 busy_timeout → 立即返回 SQLITE_BUSY
         └─ 有 busy_timeout(5000) → 休眠〜1ms → 再试
              ├─ 对方完成 → 拿到锁 → 成功
              └─ 5000ms 超时 → 返回 SQLITE_BUSY
```

等价于手写：

```c
while ((rc = sqlite3_step(stmt)) == SQLITE_BUSY) {
    usleep(1000);  /* 睡1ms再试 */
    if (++retry > 5000) break;
}
```

同理读也可以阻塞写：

```
SELECT（持 SHARED）
  └─ 写者要升级 PENDING → 等所有 SHARED 释放
        ├─ 小查询很快 → SHARED 释放 → 写者继续
        └─ 大查询很久 → 写者卡在 PENDING 自旋等待
```

**`sqlite3_busy_timeout` 不是一把锁**——是一个**自动重试回调**。当操作获取不到需要的锁级别时，代替你循环 `sleep → retry`，超时再失败。没有它，你需要自己处理 `SQLITE_BUSY` 返回值。





#### 回滚模式（默认，journal_mode=delete/truncate/persist）

```
时间线：
                                 ┌── 等待所有读者释放 SHARED
                                 │
读者1 → SHARED                   │  读者1 → UNLOCKED
读者2 → SHARED                   │  读者2 → UNLOCKED
                                 ▼
写者 → RESERVED ───────────→ PENDING ──→ EXCLUSIVE (开始写)
                                 ▲
                                 └── 阻塞所有新 SHARED
```

关键特性：
- **写者先声明 RESERVED**：告诉别人"我要写了"，但还在读旧数据准备
- **RESERVED 不阻塞读者**：其他连接可以继续读（SHARED 兼容）
- **升级到 PENDING 时才阻塞**：写者准备真正落盘，不让新读者进来
- **EXCLUSIVE 独占**：真正写入磁盘，所有读写全堵

#### WAL 模式（journal_mode=wal）

```
连接1 → SHARED (读 原文件)
连接2 → EXCLUSIVE (写 WAL 文件)      ← 读写不互斥！
连接3 → SHARED (读 原文件 + WAL)
```

WAL 模式把"写"移到单独的文件（`.db-wal`）：
- 读者：读**原文件**（已有数据）+ **WAL 文件**（最新提交）
- 写者：只往 WAL 文件追加写
- **两者不互斥**，读者不需要 STOP 写者，写者不需要等待读者

**`busy_timeout` 在 WAL 模式几乎用不上**——读写并行，没有 EXCLUSIVE 级别阻塞。

#### 一句话总结

| 模式 | 读写互斥 | 场景 |
|------|---------|------|
| 回滚（默认） | ⚠️ 写者阻塞所有读者 | 单连接、低频写入 |
| WAL | ✅ 不互斥 | 多连接、高频写入需要并发 |

#### 内部实现：锁到底是怎么实现的？

##### 锁的载体：db 文件里的 1 个页

SQLite 在**数据库文件的第1页（页头）**的开头保留了一个 32 字节的 **lock-byte page**（实际只用了最开始的几个字节）。所有锁都是通过操作系统提供的**建议性锁**（advisory lock）实现的。

SQLite 没有自己实现锁排队——它依赖操作系统内核来仲裁。

##### 实现路径

```
sqlite3_step(SELECT)
  → 哪个文件描述符上锁？
     fd = sqlite3_file_control(db, NULL, SQLITE_FCNTL_FILE_POINTER, ...)

  → posix 平台：fcntl(fd, F_SETLK/F_SETLKW, &flock)
     Windows 平台：LockFileEx / UnlockFileEx

  → 锁在 fcntl 层面映射为文件区域上的读写锁：

     SHARED:   F_RDLCK  (读锁，多个进程可同时持有)
     RESERVED: F_WRLCK  (写锁，同一把锁的特定字节区域)
     PENDING:  F_WRLCK  (另一个区域的写锁)
     EXCLUSIVE:F_WRLCK  (全范围写锁)
```

##### 5 级锁在内核层面的映射

SQLite 在文件特定偏移量上用不同的 `fcntl` 区域来代表不同锁级别：

```
.db 文件布局（示意）：

[ 页头 (100 bytes) ]
  偏移 1:   SHARED 锁区域 (1 byte)
  偏移 2:   RESERVED 锁区域 (1 byte)
  偏移 3:   PENDING 锁区域 (1 byte)
  偏移 4-7: EXCLUSIVE 锁区域 (4 bytes)

[ 第1页数据 ]
[ 第2页数据 ]
...
```

| 锁级别 | fcntl 操作 | 锁定区域 | 内核行为 |
|--------|-----------|---------|---------|
| **UNLOCKED** | `F_UNLCK` | 所有区域解锁 | 什么都不锁 |
| **SHARED** | `F_RDLCK` | SHARED 区域（偏移1） | 多个进程可同时拿读锁 |
| **RESERVED** | `F_WRLCK` | RESERVED 区域（偏移2） | 只有一个进程能拿写锁，但 SHARED 区域不受影响 |
| **PENDING** | `F_WRLCK` | PENDING 区域（偏移3） | 通知内核"我要独占了"，新的 F_RDLCK 被拒绝 |
| **EXCLUSIVE** | `F_WRLCK` | EXCLUSIVE 区域（偏移4-7） | 其他进程拿不到任何锁 |

##### 为什么是 advisory lock（建议性锁）？

```
进程A: fcntl(fd, F_SETLK, F_RDLCK)  → 拿到读锁
进程B: fcntl(fd, F_SETLK, F_WRLCK)  → 被拒绝，因为进程A持着读锁
进程B: 不走 fcntl，直接 write(fd, buf, 100)  → ❌ 这是允许的！

解决方案：SQLite 约定"每个连接都通过 fcntl 来协商"。
如果有人故意绕过 fcntl 直接 write，数据库就坏了。
所以叫"建议性锁"——建议大家都遵守，但不强制。
```

##### 死锁如何避免

```
进程A: SHARED
进程B: SHARED

进程A: 想升级 EXCLUSIVE → 等进程B 释放 SHARED
进程B: 也想升级 EXCLUSIVE → 等进程A 释放 SHARED
       → ❌ 死锁！互相等

SQLite 的解法：
  - PENDING 阶段：拒绝新 SHARED，但不阻塞已存在的 SHARED
  - fcntl 升级时用 F_SETLKW（阻塞等待）而不是 F_SETLK（失败返回）
  - 但 SQLite 用一个超时策略：F_SETLK 失败后 sleep 重试，不会一直死等
```

##### 通过 `/proc/locks` 观察

```bash
# 两个进程同时打开同一个 .db

进程1: 正在 SELECT
$ cat /proc/locks | grep sqlite
1: POSIX  ADVISORY  READ  12345 08:03:123456 1-1   # SHARED 锁

进程2: 正在 UPDATE
1: POSIX  ADVISORY  WRITE 12346 08:03:123456 2-2   # RESERVED 锁
```

可以看到操作系统层面就是 `POSIX ADVISORY READ/WRITE` 锁。

##### 简单总结

SQLite 的锁不是自己在用户态实现的，而是**委托操作系统内核的 fcntl 文件锁**：

```
用户层: SHARED → RESERVED → PENDING → EXCLUSIVE
                           ↓
内核层: F_RDLCK → F_WRLCK(region A) → F_WRLCK(region B) → F_WRLCK(region C)
              ↓
         POSIX advisory lock（由内核管理等待队列和冲突检测）
```

### 项目相关

**当前场景**：单连接、单线程、只读启动时加载 → 不需要锁，WAL 无关。但如果以后多线程多连接同时写同一个 `.db`（例如北向线程写值 + 管理页面改点表），就需要考虑上 WAL 模式。

**当前项目单连接单线程用不上**——`egw_ptable_open()` 只有一个连接，加载完就 `sqlite3_close()` 了，不存在竞争。但保留作为防御性编程，万一以后多个线程同时 open 同一个 .db 不会卡死。

### sqlite3_exec

```c
int sqlite3_exec(
    sqlite3 *db,                    /* 数据库连接 */
    const char *sql,                /* 要执行的 SQL（可包含多条，用 ; 分隔） */
    sqlite3_callback callback,      /* 每行结果的回调（不需要时传 NULL） */
    void *arg,                      /* 传给回调的用户数据 */
    char **errmsg                   /* 错误消息（不需要时传 NULL） */
);
```

一次性执行多条 SQL，不用手动 prepare/step/finalize。适合：

- DDL：`CREATE TABLE`、`ALTER TABLE`
- 批量 INSERT
- 初始化脚本

`callback` 在每条结果行时被调用，格式 `callback(arg, ncol, values[], names[])`。不需要处理结果时传 `NULL, NULL`。

**项目中的用法**：

```c
char *errmsg = NULL;
int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
if (rc != SQLITE_OK) {
    sqlite3_free(errmsg);  /* 注意：errmsg 必须用 sqlite3_free 释放 */
    return EGW_RET_CODE(ERR_PARSE);
}
```

`sql` 参数是一条大 SQL，用 `";"` 拼接三条 `CREATE TABLE IF NOT EXISTS`。

### sqlite3_prepare_v2 / sqlite3_step / sqlite3_finalize

这是 `sqlite3_exec` 内部的底层机制。`exec` 帮你包好了，但需要逐行处理结果时必须用这一组。

#### 完整执行流程

```
SQL 文本
  │
  ▼
PREPARE ──→ VDBE 字节码程序 (sqlite3_stmt *)
  │               ↑ 编译阶段：解析 SQL、检查语法语义、
  │                 生成虚拟机指令序列
  ▼
STEP ──→ 执行字节码
  │        ├─ 返回 SQLITE_ROW    ← 有一条结果行可读
  │        ├─ 返回 SQLITE_DONE   ← 执行完毕，无更多结果
  │        └─ 返回 错误码         ← 执行失败
  │
  ▼
FINALIZE ──→ 释放 stmt 内存
```

#### 各函数签名

```c
/* 1. 编译 */
int sqlite3_prepare_v2(
    sqlite3 *db,            /* 数据库连接 */
    const char *sql,        /* 单条 SQL（不能有 ; 分隔多个） */
    int nByte,              /* SQL 长度（-1 表示自动 strlen） */
    sqlite3_stmt **ppStmt,  /* OUT：编译好的语句句柄 */
    const char **pzTail     /* OUT：指向剩余未解析的 SQL（NULL 忽略） */
);

/* 2. 执行 */
int sqlite3_step(sqlite3_stmt *stmt);
/* 返回：SQLITE_ROW 有数据 / SQLITE_DONE 完成 / 错误码 */

/* 3. 释放 */
int sqlite3_finalize(sqlite3_stmt *stmt);
```

#### step 的本质

```c
int sqlite3_step(sqlite3_stmt *stmt);
```

`step` = **把游标往前推一行**。`prepare` 编译完 SQL 后，游标停在**第 0 行之前**（没有数据可读）。每次 `step` 做两件事：

1. **往前走一行**（第一次 step 走到第 1 行）
2. **看那一行有没有数据**：有 → `SQLITE_ROW`；没了（已越过最后一行）→ `SQLITE_DONE`

```
prepare 完后:
  游标位置:  ← [第0行之前]
  数据:        [行1] [行2] ... [行N]

第1次 step:
  游标位置:         [行1] ←
  数据:             [行1] [行2] ... [行N]
  返回: SQLITE_ROW  ✅ 可以读

第2次 step:
  游标位置:               [行2] ←
  数据:             [行1] [行2] ... [行N]
  返回: SQLITE_ROW  ✅

第N+1次 step（最后一行之后）:
  游标位置:                            ← [行N之后]
  数据:             [行1] [行2] ... [行N]
  返回: SQLITE_DONE ❌ 无数据了
```

类比文件读取：
| SQLite | 文件 I/O |
|--------|---------|
| `prepare` | `fopen`（打开文件） |
| `step` | `fgets` / `readdir`（读下一行/条目） |
| `column_*` | 从刚读到的那一行取字段 |
| `finalize` | `fclose`（关闭文件） |
| `reset` | `rewind`（回到开头重新读） |

**关键**：`column_*` 读取的是**当前游标位置的行**。`step` 推进游标后，上一行的列指针就失效了。

#### `sqlite3_column_*` 函数族

所有 `column_*` 函数的第一个参数都是 `sqlite3_stmt *`，第二个参数 `iCol` 是列索引（从 0 开始）。

##### 取值函数

```c
/* 整数类型 */
int              sqlite3_column_int(stmt, iCol);       /* 返回 int（32位），超出截断 */
sqlite3_int64    sqlite3_column_int64(stmt, iCol);     /* 返回 64 位完整整数 */

/* 浮点类型 */
double           sqlite3_column_double(stmt, iCol);    /* 返回 double */

/* 文本类型 */
const unsigned char *sqlite3_column_text(stmt, iCol);  /* 返回 TEXT 列的 UTF-8 字符串指针 */

/* BLOB 二进制 */
const void      *sqlite3_column_blob(stmt, iCol);      /* 返回 BLOB 二进制数据指针 */

/* 字节数 */
int              sqlite3_column_bytes(stmt, iCol);     /* TEXT/BLOB 的字节数（不含 \0） */
```

##### 元数据函数

```c
int              sqlite3_column_type(stmt, iCol);      /* 列的类型：SQLITE_INTEGER / FLOAT / TEXT / BLOB / NULL */
const char      *sqlite3_column_name(stmt, iCol);      /* 列名（SELECT 中的别名或原始列名） */
const char      *sqlite3_column_decltype(stmt, iCol);  /* CREATE TABLE 时声明的类型 */
int              sqlite3_column_count(stmt);           /* 当前结果集的列数 */
```

##### 返回值生命周期

`column_*` 返回的指针（`text` / `blob` / `name`）指向 **SQLite 内部缓冲区**，在以下情况失效：

| 操作 | 指针是否有效 |
|------|-------------|
| 再次 `sqlite3_step` 推进游标 | ❌ 上一行的指针全失效 |
| 当前行内多次调 `column_*` | ✅ 同一行的指针保持有效 |
| `sqlite3_finalize` | ❌ stmt 销毁，所有指针失效 |

**需要长期持有的字符串必须 `strdup` 拷贝出来。**

##### 类型自动转换

SQLite 是**弱类型**——存的是什么类型不重要，`column_*` 会自动转换：

| 读取函数 | 实际存的是 TEXT | 实际存的是 INTEGER |
|----------|----------------|--------------------|
| `column_text` | 直接返回字符串 | 自动转成字符串（"42"） |
| `column_int` | 自动 `atoi`（"42" → 42） | 直接返回整数 |
| `column_double` | 自动 `atof` | 自动转成 double |
| `column_type` | 返回 `SQLITE_TEXT` | 返回 `SQLITE_INTEGER` |

##### 项目中注册字段的用法

```c
/* 逐列匹配 */
for (int f = 0; f < nfield; f++) {
    int col = -1;
    for (int c = 0; c < ncol; c++) {
        if (strcmp(fields[f].name, sqlite3_column_name(stmt, c)) == 0) {
            col = c;                          ← 按列名匹配，不依赖位置
            break;
        }
    }
    if (col < 0) { write_default(...); continue; }    ← 列不存在，用默认值

    int actual = sqlite3_column_type(stmt, col);       ← 校验类型
    if (actual == SQLITE_NULL) {
        write_default(...);                            ← NULL 也用默认值
    } else {
        read_column(..., stmt, col);                   ← 按 ctype 调对应读取函数
    }
}
```

#### 绑定参数（安全防注入）

```c
/* 准备带占位符的 SQL */
sqlite3_prepare_v2(db, "INSERT INTO t VALUES (?1, ?2, ?3)", ...);

/* 绑定参数 */
sqlite3_bind_int(stmt, 1, 42);
sqlite3_bind_text(stmt, 2, "hello", -1, SQLITE_TRANSIENT);
sqlite3_bind_double(stmt, 3, 3.14);

/* 执行 */
sqlite3_step(stmt);  → SQLITE_DONE

/* 重置（复用 stmt，不重新 prepare） */
sqlite3_reset(stmt);
```

占位符格式：

| 写法 | 示例 | 说明 |
|------|------|------|
| `?` | 匿名 | 按位置匹配，第1个 `?` 是1 |
| `?NNN` | `?1` / `?2` | 按编号匹配 |
| `:AAA` | `:name` | 按名字匹配 |
| `@AAA` | `@name` | 同上 |
| `$AAA` | `$name` | 同上 |

#### 项目中验证元数据表存在的用法

```c
rc = sqlite3_prepare_v2(db, "SELECT 1 FROM egw_meta LIMIT 1", -1, &stmt, NULL);
if (rc != SQLITE_OK) {
    /* egw_meta 表不存在 → prepare 时就失败 */
    return NULL;
}
sqlite3_finalize(stmt);
/* 表存在，正常返回 */
```

这里只关心 prepare 是否成功（表是否存在），不 step 读取数据，所以直接 finalize。

#### exec 和 prepare 的对比

```
sqlite3_exec(db, sql, callback, arg, errmsg)
  ├─ 可以包含多条 SQL（自动分号分割）
  ├─ 有结果时自动调 callback
  ├─ 出错时自动停在错误处，返回 errmsg 字符串
  └─ 适合：DDL、批量操作、不需要逐行处理的场景

sqlite3_prepare_v2 + step + column + finalize
  ├─ 一次只处理一条 SQL
  ├─ 必须手动 step 每条结果行
  ├─ 可以用 column_xxx 精确读取各列值
  └─ 适合：需要逐行处理查询结果、绑定参数的场景
```

#### sqlite3_prepare_v2 对比 V1

```c
/* V1（旧的，已废弃） */
int sqlite3_prepare(sqlite3 *db, const char *sql, int nByte,
                    sqlite3_stmt **ppStmt, const char **pzTail);

/* V2（当前，推荐） */
int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int nByte,
                       sqlite3_stmt **ppStmt, const char **pzTail);
```

差别不在 `prepare` 本身，在 `step` 的行为：

| 场景 | V1 | V2 |
|------|----|----|
| `step` 遇到 `SQLITE_SCHEMA` | 直接抛给调用方，你必须重新 prepare | 内部自动 re-prepare 重试，对调用方透明 |
| `sqlite3_column_decltype` | ❌ | ✅ |
| 扩展错误码 | ❌ | ✅ |

V1 早已废弃。项目直接用 V2 即可。

### ensure_schema

```c
static egw_err_t ensure_schema(sqlite3 *db);
```

作用：**确保三张点表存在，不存在则创建**。

`egw_ptable_open()` 中 `ensure_schema` 的执行序列：

```
1. sqlite3_open_v2("test_config.db")    ← 文件还不存在，创建空文件
2. ensure_schema(db)                    ← 在这个空文件里建三张表
     ├─ sqlite3_exec(CREATE TABLE IF NOT EXISTS southbound(...))
     ├─ sqlite3_exec(... northbound(...))
     └─ sqlite3_exec(... route(...))
3. load_southbound(db)                  ← 现在表存在了，可以 SELECT
```

本质是 **schema-on-first-use** 模式：第一次运行时 `.db` 是空的，`ensure_schema` 建好表结构；后续执行时 `.db` 已有表，`CREATE TABLE IF NOT EXISTS` 什么都不做直接返回。

```sql
CREATE TABLE IF NOT EXISTS southbound (...)
--             ↑ 这个 IF NOT EXISTS 是关键——第二次运行时不会报错
```

### 项目相关

`egw_ptable_open()` 在 `src/ptable/egw_ptable.c:283` 调用：
- flags = `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE`（读写 + 自动创建）
- VFS = NULL（默认）
- 成功后设置 `sqlite3_busy_timeout(db, 5000)` 避免并发时立即返回 `SQLITE_BUSY`
- 加载完三表后立即 `sqlite3_close(db)`，运行时不再使用 SQLite

## 多条 SQL 执行 vs 单条合并 SQL

### 场景

在 `egw_ptable_open` 中对三件事分别执行不同回调：

| 任务 | 回调行为 |
|------|---------|
| 发现有效表（两边都存在） | `discover_cb` → 填写 `tables[]` |
| egw_meta 有但表不存在 | `warn_cb` → 打 `EGW_LOGW` |
| 表存在但 egw_meta 没有 | `debug_cb` → 打 `EGW_LOGD` |

### 第一条 SQL（当前方案）

```sql
-- 三条独立，每条约 3 行
SELECT m.key, m.value FROM egw_meta m
WHERE EXISTS (SELECT 1 FROM sqlite_master WHERE type='table' AND name = m.key)
  AND m.key != 'schema_version';

SELECT key FROM egw_meta WHERE key != 'schema_version'
  AND NOT EXISTS (SELECT 1 FROM sqlite_master WHERE type='table' AND name = key);

SELECT name FROM sqlite_master WHERE type='table'
  AND name != 'egw_meta'
  AND name NOT IN (SELECT key FROM egw_meta WHERE key != 'schema_version');
```

### 第二条 SQL（合并 UNION ALL）

```sql
-- 一条，含 UNION ALL + CASE + 3 段 SELECT，约 15 行
SELECT m.key, m.value, s.name,
  CASE WHEN s.name IS NULL THEN 'meta_only' ELSE 'valid' END AS tag
FROM egw_meta m
LEFT JOIN sqlite_master s ON s.type='table' AND s.name = m.key
WHERE m.key != 'schema_version'

UNION ALL

SELECT sm.name, NULL, NULL, 'db_only' AS tag
FROM sqlite_master sm
WHERE sm.type='table' AND sm.name != 'egw_meta'
  AND NOT EXISTS (SELECT 1 FROM egw_meta m WHERE m.key = sm.name)
ORDER BY tag;
```

### 对比

| 维度 | 三条独立 SQL | 单条 UNION ALL |
|------|------------|---------------|
| **可读性** | ⭐⭐⭐ 每条约 3-5 行，意图一目了然 | ⭐⭐ 15 行，UNION + CASE + 多个子查询，开发调试需要大脑解包 |
| **维护** | ⭐⭐⭐ 改一个规则单独改一条，不影响其他 | ⭐ 改一处可能牵扯整条 SQL 结构，C 字符串拼 SQL 容易缺空格/引号 |
| **回调** | ⭐⭐⭐ 每条 SQL 一个简单回调（3-5 行） | ⭐ 一个回调要 `strcmp(tag)` 判断三种场景，回调逻辑变复杂 |
| **性能（SQLite）** | ⭐ 3 次 `exec()` → 3 次 `prepare` + 3 次语法分析 + B-tree 遍历 | ⭐⭐⭐ 1 次 | 但三张表都很小（几张），差距微秒级 |
| **性能（事件循环）** | ⭐ 不适用（只启动时跑一次） | ⭐⭐ 同上 |
| **事务性** | ⭐ 三条独立，中间可能有并发写入改变结果 | ⭐⭐⭐ 一条语句，结果集是快照一致的 |

### 结论

| 场景 | 推荐 |
|------|------|
| 启动时初始化、只跑一次、表很小 | **多条 SQL**（可读性 > 性能） |
| OLTP 高频查询、大表、需要快照一致 | **单条 UNION**（性能 + 事务性） |
| 不同结果需要走**不同逻辑**（填数组 vs 打日志 vs 提示） | **多条 SQL**（每个回调各司其职） |

当前场景（`egw_ptable_open` 启动加载）满足三个条件：
1. 只跑一次
2. 表很小（< 10 张）
3. 不同结果走不同逻辑

→ **三条独立 SQL 是正确的选择**。性能无差别，可读性大幅提升。

## 查看 .db 文件

### 命令行工具 sqlite3

```bash
# 打开交互式 shell
sqlite3 config.db

# 一行命令查询
sqlite3 config.db "SELECT * FROM egw_head"

# 格式化输出
sqlite3 -header -column config.db "SELECT * FROM egw_head"
```

### 常用命令

```
sqlite> .tables                ← 列出所有表
sqlite> .schema egw_head       ← 查看表结构
sqlite> .headers on            ← 显示列名
sqlite> .mode column           ← 列对齐输出
sqlite> .dump egw_head         ← 导出表为 SQL
sqlite> .quit                  ← 退出
```

### 查看二进制文件头

SQLite `.db` 文件前 16 字节固定为魔数，可用 `xxd`/`hexdump` 验证：

```bash
xxd config.db | head -3
```

前 16 字节：`53 51 4c 69 74 65 20 66 6f 72 6d 61 74 20 33 00`
= ASCII `"SQLite format 3\0"`

### 项目中头检测

```c
/* egw_head 根节点校验等价于魔数检测 */
SELECT desc FROM egw_head WHERE id=1 AND type='HEAD'
```

如果 SQLite 文件能打开但查询不到根节点 → 文件不是合法的 egw 格式数据库。
