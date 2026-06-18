# 嵌入式 C 项目日志系统方案对比

## 1. printf / fprintf（当前方案）

```c
printf("warning: table '%s' not found\n", name);
```

| 特性 | 评价 |
|------|------|
| 复杂度 | ⭐ 零依赖，一行代码 |
| 日志级别 | ❌ 无 |
| 开关控制 | ❌ 无 |
| 性能 | 差（printf 是系统调用，每次写 stdout） |
| 远程 | ❌ 无 |
| 适合场景 | 原型验证、调试阶段 |

**痛点**：无法关掉调试日志、没有级别过滤、生产环境不能禁用（除非重定向 stdout 到 /dev/null）。

---

## 2. syslog（POSIX 标准）

```c
#include <syslog.h>

openlog("edgegw", LOG_PID | LOG_NDELAY, LOG_DAEMON);
syslog(LOG_ERR, "table '%s' not found", name);
closelog();
```

| 特性 | 评价 |
|------|------|
| 复杂度 | ⭐ 系统自带，零依赖 |
| 日志级别 | ✅ `LOG_ERR / LOG_WARNING / LOG_INFO / LOG_DEBUG` |
| 开关控制 | ✅ `/etc/rsyslog.conf` 按程序名/级别过滤 |
| 性能 | 中等（通过 /dev/log socket 发送） |
| 远程 | ✅ rsyslog 配置即可发到远程服务器 |
| 适合场景 | Linux 守护进程、生产环境 |

**系统调用过程**：

```
syslog(LOG_WARNING, "msg")
  → openlog() 打开 /dev/log 或 /run/systemd/journal/socket
  → sendto() 发送结构化消息
  → syslogd/journald 接收 → 写入 /var/log/syslog 或 journal
```

查看日志：

```bash
grep edgegw /var/log/syslog          # rsyslog
journalctl _COMM=edgegw              # journald
```

---

## 3. zlog（纯 C 日志库）

```c
#include <zlog.h>

zlog_category_t *c = zlog_get_category("ptable");
zlog_warn(c, "table '%s' not found", name);
```

通过配置文件控制输出：

```conf
# /etc/edgegw/zlog.conf
[global]
buffer = false

[ptable]
level = WARN
file = /var/log/edgegw-ptable.log

[app]
level = INFO
```

| 特性 | 评价 |
|------|------|
| 复杂度 | ⭐⭐ 追加一个 .c 到构建 |
| 日志级别 | ✅ `FATAL / ERROR / WARN / INFO / DEBUG` |
| 开关控制 | ✅ 运行时重读配置（`SIGHUP`），不改代码 |
| 性能 | ⭐⭐⭐ 优秀（无锁 per-thread buffer，异步写） |
| 远程 | ❌ 无（但可以配 pipe 到 syslog） |
| 适合场景 | 需要运行时调日志级别的嵌入式产品 |

---

## 4. 自定义日志宏（项目中常见）

```c
/* egw_log.h */
#define LOG_ERR   0
#define LOG_WARN  1
#define LOG_INFO  2
#define LOG_DEBUG 3

extern int g_log_level;

#define egw_log(level, fmt, ...) \
    do { \
        if ((level) <= g_log_level) { \
            fprintf(stderr, "[%d] " fmt "\n", (level), ##__VA_ARGS__); \
        } \
    } while (0)
```

用法：

```c
egw_log(LOG_WARN, "table '%s' not found", name);
```

改成 zlog 时只需改宏定义，调用点不变。

| 特性 | 评价 |
|------|------|
| 复杂度 | ⭐ 几十行宏 |
| 日志级别 | ✅ 自定 |
| 开关控制 | ✅ 编译期（宏开关）/ 运行期（全局变量） |
| 性能 | 差（条件判断 + fprintf） |
| 远程 | ❌ |
| 适合场景 | 不想加第三方依赖的中小型项目 |

---

## 5. 对比总表

| | printf | syslog | zlog | 自定义宏 |
|------|--------|--------|------|---------|
| 依赖 | 无 | libc | zlog.c | 无 |
| 代码行数 | 1行 | 3行 | 2行 + 配置文件 | 头文件宏 |
| 级别过滤 | ❌ | ✅ | ✅ | ✅ |
| 运行时调级别 | ❌ ✅ (rsyslog.conf) | ✅ | ✅ |
| 性能 | 差 | 中 | 好 | 差 |
| 远程日志 | ❌ | ✅ | ❌ | ❌ |
| 文件大小 | 0 | 0 | ~3KB | 0 |

## 6. 项目当前状态

当前在 `egw_ptable.c` 里用 `printf` 打 warning。等将来确定日志方案后，把 `discover_cb` 里的 `printf` 改成对应的日志宏或者 syslog 调用即可。不阻塞当前开发。

## 参考资源

- syslog man page：`man 3 syslog`
- zlog 仓库：https://github.com/HardySimpson/zlog
- Linux 日志架构：`Documentation/admin-guide/sysrq.rst`
