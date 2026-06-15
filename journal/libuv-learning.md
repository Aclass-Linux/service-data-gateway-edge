# libuv 学习笔记

## 核心概念

libuv 是一个异步 I/O 库，核心是一个 **event loop**（事件循环）。

```
你的代码（同步）
  │
  │ 注册异步操作：uv_read_start、uv_timer_start 等
  ▼
uv_run                        ← 进入事件循环
  │
  ├── epoll_wait（或 kqueue/IOCP）← 阻塞，等事件
  │
  └── 事件来了，回调你的函数
        └── on_read、on_timer、on_close
```

三个核心概念：

| 概念 | 类比 | 说明 |
|------|------|------|
| **Loop** | 工厂的流水线 | 不断运转，处理各种任务 |
| **Handle** | 工位上的设备 | 有状态的东西（timer、pipe、tcp 等） |
| **Request** | 一张任务单 | 一次性操作（write、connect、getaddrinfo） |

---

## Loop — 事件循环

### `uv_default_loop()` — 获取全局单例

```c
uv_loop_t *uv_default_loop(void);
```

| 返回 | 说明 |
|------|------|
| `uv_loop_t *` | 进程唯一的默认 event loop |

**行为**：

```
第一次调用 → 创建 loop（epoll_create1、初始化队列等）
第二次调用 → 返回同一个 loop
```

等价于：

```c
static uv_loop_t *default_loop = NULL;

uv_loop_t *uv_default_loop(void) {
    if (!default_loop) {
        default_loop = malloc(sizeof(uv_loop_t));
        uv_loop_init(default_loop);       // epoll_create1 + 初始化
    }
    return default_loop;
}
```

**为什么需要单例？**

多个模块（timer、串口、TCP）都需要注册到同一个 loop，否则每个 loop 各跑各的，无法统一调度。

### `uv_loop_init()` — 创建新 loop

如果你需要独立 loop（比如测试隔离）：

```c
uv_loop_t *loop = malloc(sizeof(uv_loop_t));
uv_loop_init(loop);          // 内部调 epoll_create1
// 用完必须释放
uv_loop_close(loop);
free(loop);
```

### `uv_run()` — 启动事件循环

```c
int uv_run(uv_loop_t *loop, uv_run_mode mode);
```

**mode 三种模式：**

| 模式 | 行为 | 常用场景 |
|------|------|---------|
| `UV_RUN_DEFAULT` | 阻塞运行，直到 loop 中没有活跃 handle/request | 主循环 |
| `UV_RUN_ONCE` | 阻塞等一个事件，处理完就返回 | 集成到其他事件循环 |
| `UV_RUN_NOWAIT` | 非阻塞，处理完已有事件就返回 | 轮询 |

**返回值：**

| 返回值 | 含义 |
|--------|------|
| `0` | loop 中已无活跃句柄，正常退出 |
| `非 0` | 仍有活跃句柄在运行（`UV_RUN_ONCE`/`NOWAIT` 时可能） |

**伪代码实现：**

```c
int uv_run(uv_loop_t *loop, uv_run_mode mode) {
    while (uv__loop_alive(loop)) {           // 还有活跃句柄？
        uv__update_time(loop);               // 更新缓存时间
        uv__run_timers(loop);                // 处理到期的定时器
        uv__io_poll(loop, timeout);          // epoll_wait 阻塞等 I/O
        uv__run_closing_handles(loop);       // 处理关闭回调
    }
    return 0;
}
```

### `uv_loop_close()` — 关闭 loop

```c
int uv_loop_close(uv_loop_t *loop);
```

调用前必须确保所有 handle 已被关闭，否则返回 `UV_EBUSY`。

---

## Handle — 有状态的对象

Handle 是 libuv 的核心抽象。所有 handle 都以 `uv_xxx_t` 命名，通过父子类型构成体系：

```
uv_handle_t（所有 handle 的基类）
  ├── uv_stream_t（流）
  │     ├── uv_tcp_t（TCP 连接）
  │     ├── uv_pipe_t（管道/串口）
  │     └── uv_tty_t（终端）
  ├── uv_udp_t（UDP）
  ├── uv_timer_t（定时器）
  ├── uv_idle_t（空闲回调）
  ├── uv_signal_t（信号）
  ├── uv_poll_t（自定义 fd 轮询）
  ├── uv_fs_event_t（文件系统事件）
  └── uv_fs_poll_t（文件系统轮询）
```

### Handle 生命周期

```
uv_xxx_init(loop, &handle)    ← 初始化（分配、注册到 loop）
  │
  │ 可选：uv_xxx_open(handle, fd) 或 uv_xxx_bind/send 等
  │
  │ uv_xxx_start(handle, cb)  ← 开始（注册到 epoll/系统）
  │
  ▼
事件到达 → 调 cb → 处理
  │
  │ uv_xxx_stop(handle)  ← 停止
  │ 或 uv_close(handle, close_cb)  ← 关闭
  │
  ▼
close_cb 被调 → 资源已释放
```

### uv_close — 所有 handle 的统一关闭方式

```c
void uv_close(uv_handle_t *handle, uv_close_cb cb);
```

**关键特性**：

- **异步**：`uv_close` 不会立即释放，而是把 handle 放入关闭队列
- **关闭回调**：资源完全释放后调用 `cb`
- **必须调**：所有 handle 最终必须 `uv_close`，否则内存泄漏

```c
uv_close((uv_handle_t *)&pipe, on_close);

void on_close(uv_handle_t *handle) {
    // 此时 handle 已彻底关闭
    // 如果 handle 是 malloc 的，在这里 free
    free(handle);
}
```

---

## 使用示例

### 例 1：定时器

```c
#include <uv.h>

uv_timer_t timer;

void on_timer(uv_timer_t *handle) {
    printf("每秒触发一次\n");
}

int main(void) {
    uv_timer_init(uv_default_loop(), &timer);
    uv_timer_start(&timer, on_timer, 0, 1000);  // 立即开始，每秒重复
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);   // 启动循环
    return 0;
}
```

### 例 2：异步读写（pipe/串口）

```c
#include <uv.h>
#include <stdio.h>

uv_pipe_t pipe;
uv_buf_t buf;

void on_alloc(uv_handle_t *handle, size_t suggested, uv_buf_t *buf) {
    buf->base = malloc(suggested);
    buf->len  = suggested;
}

void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        printf("读到 %zd 字节\n", nread);
    }
    free(buf->base);

    if (nread < 0) {            // 错误或 EOF
        uv_close((uv_handle_t *)stream, NULL);
    }
}

void on_write(uv_write_t *req, int status) {
    free(req);
}

void write_data(uv_stream_t *stream, const char *data, size_t len) {
    uv_write_t *req = malloc(sizeof(uv_write_t));
    uv_buf_t buf = uv_buf_init((char *)data, (unsigned int)len);
    uv_write(req, stream, &buf, 1, on_write);
}

int main(void) {
    uv_loop_t *loop = uv_default_loop();

    int fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY);
    uv_pipe_init(loop, &pipe, 0);
    uv_pipe_open(&pipe, fd);                // 绑定 fd
    uv_read_start((uv_stream_t *)&pipe, on_alloc, on_read);  // 开始监听

    write_data((uv_stream_t *)&pipe, "hello", 5);

    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}
```

### 例 3：自定义 fd 轮询（uv_poll_t）

如果 libuv 没有封装你需要的 handle 类型，可以用 `uv_poll_t` 直接轮询任意 fd：

```c
uv_poll_t poller;

void on_poll(uv_poll_t *handle, int status, int events) {
    if (events & UV_READABLE) {
        // fd 可读
    }
    if (events & UV_WRITABLE) {
        // fd 可写
    }
}

int main(void) {
    int fd = open("/dev/ttyS0", O_RDWR);

    uv_poll_init(uv_default_loop(), &poller, fd);
    uv_poll_start(&poller, UV_READABLE, on_poll);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    return 0;
}
```

`uv_poll_t` 在上层等价于 `uv_pipe_t` 的收发功能，但不带 stream 层缓冲和回调封装。

---

## Request — 一次性操作

| Request 类型 | 用途 |
|-------------|------|
| `uv_write_t` | 写数据（一次性） |
| `uv_connect_t` | TCP 连接（一次性） |
| `uv_getaddrinfo_t` | DNS 解析（一次性） |
| `uv_shutdown_t` | 关闭写端（一次性） |
| `uv_fs_t` | 文件操作（一次性） |

所有 Request 的通用模式：

```c
// 1. 分配（通常在栈上或 malloc）
uv_write_t req;

// 2. 发起
uv_write(&req, stream, &buf, 1, on_write);

// 3. 回调中释放
void on_write(uv_write_t *req, int status) {
    // 操作完成，如果需要 free(req)
}
```

---

## uv_pipe_t 详解

项目中使用的串口就是 `uv_pipe_t`。

### 初始化

```c
uv_loop_t *loop = uv_default_loop();
uv_pipe_t pipe;

uv_pipe_init(loop, &pipe, 0);   // ipc=0（非进程间通信）
```

`uv_pipe_init` 内部：
1. 把 pipe 注册到 loop 的 handle 链表
2. 设置 `type = UV_NAMED_PIPE`
3. 设置 `io_watcher.cb = uv__stream_io`（libuv 内部 stream 处理函数）

### 绑定 fd

```c
uv_pipe_open(&pipe, fd);   // 把串口 fd 绑定到 pipe
```

此时 pipe 的 `io_watcher.fd = fd`，但还没进 epoll。

### 开始监听

```c
uv_read_start((uv_stream_t *)&pipe, alloc_cb, read_cb);
```

执行后：
1. 设置 `stream->read_cb = read_cb`、`stream->alloc_cb = alloc_cb`
2. 调 `uv__io_start(loop, &io_watcher, POLLIN)` → `epoll_ctl(EPOLL_CTL_ADD)`

### 写数据

```c
uv_write_t req;
uv_buf_t buf = uv_buf_init("hello", 5);
uv_write(&req, (uv_stream_t *)&pipe, &buf, 1, on_write);
```

写是**异步**的：`uv_write` 把数据放入内核发送缓冲区就返回，`on_write` 在数据发送完成后被调。

### 关闭

```c
uv_close((uv_handle_t *)&pipe, on_close);
```

关闭是**异步**的：`uv_close` 把 handle 放入关闭队列，`on_close` 在资源释放后调用。

---

## 完整数据流（串口）

```
egw_serial_do_open
  │
  ├── uv_default_loop()          → 获取全局 loop
  │     └── epoll_create1()
  │
  ├── uv_pipe_init(loop, &pipe)  → 初始化 pipe handle
  │     └── uv__stream_init()
  │           └── io_watcher.cb = uv__stream_io
  │
  ├── uv_pipe_open(&pipe, fd)    → 绑定串口 fd
  │
  ├── uv_read_start(stream, on_alloc, on_read)
  │     └── uv__io_start(loop, &io_watcher, POLLIN | POLLET)
  │           └── epoll_ctl(ADD, fd, EPOLLIN | EPOLLET)
  │
  └── uv_run(loop, UV_RUN_DEFAULT)
        └── uv__io_poll()
              └── epoll_wait(loop->backend_fd, events, N, -1)
                    ↓ 串口有数据
              uv__stream_io(loop, &io_watcher, POLLIN)
                    ↓
              uv__read(stream)
                    ↓
              read(fd, buf, len)
                    ↓
              stream->read_cb = on_read(buf, len)
```

---

## 完整 Handle 类型一览

| Handle | 触发时机 | 项目使用 | 典型场景 |
|--------|----------|---------|---------|
| `uv_timer_t` | 超时/周期到达 | ✅ timer 调度 | 采集周期、心跳、超时检测 |
| `uv_prepare_t` | 每轮 I/O poll **之前** | ✅ prepare→FSM tick | 状态机驱动、预检 |
| `uv_check_t` | 每轮 I/O poll **之后** | ❌ | 收尾统计、batch 提交 |
| `uv_idle_t` | 每轮迭代（无 I/O 也跑） | ❌ | 后台低保任务 |
| `uv_poll_t` | 自定义 fd 就绪 | ✅ 串口读写 | 任意 fd 的读写事件 |
| `uv_signal_t` | POSIX 信号到达 | ✅ SIGINT | 优雅关闭、SIGHUP 重载 |
| `uv_async_t` | 跨线程 `uv_async_send()` | ❌ **规划中** | 持久化线程通知、MQTT 事件 |
| `uv_tcp_t` | TCP 连接 | ❌ | 北向 MQTT 连接 |
| `uv_pipe_t` | 管道/串口流 | ❌（改用 uv_poll）| 命名管道、进程通信 |
| `uv_udp_t` | UDP 数据报 | ❌ | 日志上报、NTP |
| `uv_fs_event_t` | 文件/目录变更 | ❌ | 配置文件热加载 |
| `uv_fs_poll_t` | 文件变更轮询 | ❌ | 配置文件定期检测 |
| `uv_process_t` | 子进程 | ❌ | 启动外部工具 |
| `uv_work_t` | 线程池任务 | ❌ | 阻塞计算（非 handle，是 request)|

---

## 各 Handle 详解

### `uv_prepare_t` — I/O poll 之前

**触发时机**：每轮迭代，epoll_wait 阻塞之前。不管有没有 I/O 事件，prepare **一定会触发**。

**生命周期的位置**（`uv_run` 一轮迭代的第 3 步）：
```
uv__update_time → uv__run_timers → uv__run_prepare → uv__io_poll → uv__run_check → uv__run_closing_handles
                                                   ↑
                                             你要做的事
```

**当前项目用法**（`src/app/gateway_engine.c`）：
```c
static void on_prepare_cb(uv_prepare_t *handle)
{
    gw_engine_t *eng = (gw_engine_t *)handle->data;
    egw_event_t  tick = { .sig = EGW_ENGINE_TICK, .data = NULL };
    egw_fsm_dispatch(&eng->fsm, &tick);
}
```

每轮迭代开始前，给 FSM 一个心跳机会。FSM 可以在这个 tick 中做周期性检查（超时检测、重试判断等），而不需要依赖 timer 回调。

**应用案例**：

- **FSM 心跳驱动**：状态机在每个 I/O 轮次前获得执行机会（当前用法）
- **超时检测**：检查某个操作是否超时，超时则投递超时事件到 FSM
- **统计采样**：每轮开始前记录时间戳，结束时用 check 计算耗时
- **连接池健康检查**：对空闲连接做探活，决定是否需要重连

---

### `uv_check_t` — I/O poll 之后

**触发时机**：每轮迭代，epoll_wait 返回且所有 I/O 回调处理完之后。和 prepare 对称。

**位置**（第 5 步）：
```
uv__run_prepare → uv__io_poll → uv__run_check → uv__run_closing_handles
                                 ↑
                           你要做的事
```

**示例**：
```c
void on_check_cb(uv_check_t *handle)
{
    engine_t *eng = handle->data;
    eng->stats.loops++;
    if (eng->stats.frames_this_loop > 0) {
        LOG_DEBUG("本轮处理 %d 帧", eng->stats.frames_this_loop);
        eng->stats.frames_this_loop = 0;
    }
}
```

**应用案例**：

- **批处理刷写**：一轮 I/O 中可能有多帧数据到达，check 中一次性刷写持久化
- **延迟统计**：记录本轮所有 I/O 事件处理完后的延迟分布
- **批处理提交**：消息队列积攒一帧帧数据，check 回调中一次性入队
- **资源回收**：每轮结束后检查是否需要释放临时资源

**prepare vs check 对比**：

```
prepare → "FSM 准备好了吗？要不要做什么决策？"
check   → "本轮 I/O 都处理了，统计一下，然后开始下一轮"
```

---

### `uv_idle_t` — 空闲回调

**触发时机**：每轮迭代空闲阶段（没有其他 handle 要处理时）。如果 I/O 繁忙，idle 可能很长时间不触发。

**位置**（介于 prepare 和 I/O poll 之间，但只在 loop 空闲时跑）：
```
uv__run_prepare → [uv__run_idle →] uv__io_poll
                     ↑ 仅当没有 I/O 待处理
```

**示例**：
```c
void on_idle_cb(uv_idle_t *handle)
{
    /* 没有 I/O 事件时做低优先级后台任务 */
    log_flush_pending();  /* 刷日志缓冲区 */
}
```

**应用案例**：

- **后台日志刷写**：没有 I/O 时把日志缓冲区刷到磁盘
- **增量 GC**：嵌入式 Lua 的渐进式垃圾回收步进
- **空闲检测**：连续 N 轮无 I/O 判定为空闲，可以降功耗
- **调试输出**：持续输出当前状态摘要（慎用，影响性能）

**注意**：idle 在 prepare 之后、I/O poll 之前，但没有固定频率。如果一定要每轮都跑，用 prepare 而非 idle。

---

### `uv_timer_t` — 定时器

**触发时机**：定时或周期到达。在 `uv__run_timers(loop)` 阶段同步回调。

**位置**（第 2 步，prepare 之前）：
```
uv__update_time → uv__run_timers → uv__run_prepare → uv__io_poll
                  ↑
            你要做的事
```

**当前项目用法**（`src/app/gateway_app.c`）：
```c
uv_timer_init(&eng->loop, &app->sched_timer);
uv_timer_start(&app->sched_timer, on_timer_cb, 1000, 1000);
```
每秒触发一次，驱动串口采集调度。

**应用案例**：

- **采集周期**：周期性发起 Modbus 请求（当前用法）
- **心跳**：定期发送心跳包保持 TCP 连接
- **超时**：启动一次性 `onetime` 定时器作为操作超时
- **延迟执行**：延迟 N 毫秒后执行某个操作，不重复
- **看门狗**：定期检查系统状态，超时无响应则重启

**`uv_timer_start` 签名**：
```c
int uv_timer_start(uv_timer_t *handle,
                   uv_timer_cb cb,       /* 回调函数 */
                   uint64_t timeout,      /* 首次触发延迟 (ms) */
                   uint64_t repeat);     /* 重复间隔 (ms)，0=一次性 */
```

---

### `uv_poll_t` — 自定义 fd 轮询

**触发时机**：被轮询的 fd 变为可读或可写（与 epoll 等价）。

**位置**（第 4 步 `uv__io_poll` 内部，epoll_wait 返回后触发）。

**当前项目用法**（`src/app/gateway_app.c`）：
```c
uv_poll_init(&app->eng.loop, &pc->poll, egw_serial_get_fd(tp));
uv_handle_set_data((uv_handle_t *)&pc->poll, pc);
uv_poll_start(&pc->poll, UV_READABLE, on_poll_cb);
```
每个串口一个 `uv_poll_t`，在回调中通过 handle->data 拿到 `port_ctx_t`。

**应用案例**：

- **串口读写**（当前用法）
- **TCP 客户端连接**：监听 socket fd 的可读/可写事件
- **eventfd**：跨线程通知，通过 `uv_poll` 监听 eventfd
- **信号 fd**：`signalfd` 代替 `uv_signal_t`
- **自定义设备驱动**：任何非标准 fd 的 I/O

**uv_poll_t vs uv_pipe_t 选择**：当 libuv 没有封装你需要的 fd 类型时（串口、eventfd、自定义驱动），用 `uv_poll_t` 自己管理读写。如果只是标准的 TCP/UDP/管道，用 `uv_tcp_t`/`uv_pipe_t` 可以获得更高层 API（背压、自动缓冲）。

---

### `uv_signal_t` — POSIX 信号

**触发时机**：注册的 POSIX 信号到达时，在 `uv__io_poll` 内部通过 signalfd 机制处理。

**当前项目用法**（`src/app/gateway_app.c`）：
```c
uv_signal_init(&app->eng.loop, &app->sigint);
uv_signal_start(&app->sigint, on_sigint_cb, SIGINT);
```

收到 Ctrl+C 后投递 `EV_SIGINT` 到 FSM → `st_running` 转移到 `st_shutdown` → `uv_stop`。

**应用案例**：

- **Ctrl+C 优雅关闭**（当前用法）
- **SIGHUP 重载配置**：收到 SIGHUP 后重新加载 `config.json`
- **SIGUSR1/SIGUSR2**：运行时切换日志级别、触发状态转储
- **SIGCHLD**（进程退出通知，结合 `uv_process_t`）

**注意**：libuv 的 signal 处理使用 signalfd（Linux）而非传统 signal 回调，因此不会中断阻塞的系统调用，信号处理函数中可以做更多事情。

---

### `uv_async_t` — 跨线程通知

**触发时机**：其他线程调用 `uv_async_send()` 后，在主线程的下一轮 I/O poll 中被处理。

**位置**：I/O poll 内部，和普通 I/O 事件一样处理。

**示例**：
```c
/* 主线程初始化 */
uv_async_t async;
uv_async_init(loop, &async, on_async_cb);

/* 后台线程触发 */
uv_async_send(&async, NULL);  /* 线程安全，不阻塞 */

/* 主线程回调（在主线程执行） */
void on_async_cb(uv_async_t *handle)
{
    msg_t *msg = pop_message();
    handle_message(msg);
}
```

**应用案例**：

- **持久化线程通知**：落盘完成后通知主线程"可以继续写新值"
- **MQTT 客户端通知**：MQTT 线程收到响应后唤醒主线程
- **Lua 脚本通知**：脚本执行完成、获取到数据后通知主线程
- **跨线程队列**：生产者线程 enqueue + `uv_async_send`，消费者在回调中 dequeue

**关键特性**：
- `uv_async_send` 可重入、线程安全，不阻塞
- 多次调用 `uv_async_send` 可能合并为一次回调（不会丢失，但只保证至少一次）
- 因此不要依赖回调次数，用计数器或队列来保证消息完整性

---

### `uv_tcp_t` — TCP

**示例**（北向 MQTT 连接场景）：
```c
uv_tcp_t client;
uv_tcp_init(loop, &client);

struct sockaddr_in addr;
uv_ip4_addr("10.0.0.1", 1883, &addr);

uv_tcp_connect(&req, &client, (struct sockaddr *)&addr, on_connect);
```

**应用案例**：

- **MQTT 客户端连接 Broker**：未来的北向通道
- **HTTP 客户端**：上报数据到 REST 接口
- **TCP 服务器**：接受外部管理连接

---

### `uv_fs_event_t` / `uv_fs_poll_t` — 文件变更

**uv_fs_event_t**：依赖于 inotify（Linux），文件变更时立即回调。
**uv_fs_poll_t**：独立于操作系统，通过轮询文件修改时间判定变更。

**应用案例**：

- **配置文件热加载**：`config.json` 修改后自动重新加载
- **点表文件监控**：`.bin` 点表文件更新后重新 mmap
- **日志文件轮转**：检测到日志文件被切分后重新打开

---

### 运行阶段关系

```
uv_run 一轮迭代：
 ① uv__update_time          缓存当前时间（timer 用）
 ② uv__run_timers           处理到期的 timer
 ③ uv__run_prepare          ★ prepare → 每轮迭代前
 ④ uv__io_poll / epoll_wait 阻塞等事件（timer、poll、signal 在此触发）
 ⑤ uv__run_check            ★ check → 每轮迭代后
 ⑥ uv__run_closing_handles  处理 uv_close 的完成回调
```

```
                ┌──────── uv_prepare_cb ────────┐
                │  每轮迭代前                    │
                │  → FSM(TICK)，驱动状态机决策   │
                └────────────────┬───────────────┘
                                 │
                      uv__io_poll / epoll_wait
                      (timer / poll / signal 在此触发)
                                 │
                ┌──────── uv_check_cb ──────────┐
                │  每轮迭代后                    │
                │  → I/O 已完成，批量刷写/统计   │
                └────────────────────────────────┘
```

---

### 三种获取数据的模式

| 模式 | 用法 | 适用场景 |
|------|------|---------|
| **直接参数** | `void on_timer(uv_timer_t *h)` | 单例全局（如 test_egw_loop.c） |
| **handle->data** | `uv_handle_set_data/get_data` | 每个 handle 有不同上下文（如 poll→port_ctx） |
| **FSM dispatch** | callback → `egw_fsm_dispatch` | 状态机驱动的生命周期管理 |

**当前项目混合策略**：poll/timer 用 `handle->data` 传 port_ctx，prepare/signal 用 FSM dispatch 控制生命周期。

---

## 参考

- libuv 官方文档：http://docs.libuv.org
- libuv 源码：`src/unix/`（Linux 实现）
- 《An Introduction to libuv》：https://nikhilm.github.io/uvbook/
