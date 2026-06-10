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

## 参考

- libuv 官方文档：http://docs.libuv.org
- libuv 源码：`src/unix/`（Linux 实现）
- 《An Introduction to libuv》：https://nikhilm.github.io/uvbook/
