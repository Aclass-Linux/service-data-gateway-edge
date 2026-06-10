# epoll 学习笔记

## 什么是 epoll

Linux 内核提供的 I/O 事件通知机制。让一个进程能**同时监视多个 fd**，有事件时内核主动通知你，不用轮询。

---

## 三个系统调用详解

### 1. `epoll_create1` — 创建 epoll 实例

```c
#include <sys/epoll.h>

int epoll_create1(int flags);
```

| 参数 | 说明 |
|------|------|
| `flags` | `0` 或 `EPOLL_CLOEXEC` |

| 返回值 | 含义 |
|--------|------|
| `> 0` | epoll 实例的文件描述符 |
| `-1` | 失败，errno 指示错误（`ENOMEM`、`EMFILE`、`ENFILE`） |

### 内核内部实现（简化）

```
用户态调用 epoll_create1(0)
  ↓
内核 sys_epoll_create1()
  ├── 1. 分配 struct eventpoll（ep 对象）
  │     ├── ep->wq: 等待队列（epoll_wait 阻塞在这）
  │     ├── ep->poll_wait: poll 回调链表
  │     ├── ep->rbr: 红黑树根（存所有注册的 fd）
  │     └── ep->rdllist: 就绪链表（有事件发生的 fd）
  │
  ├── 2. 分配 struct file 关联到 ep
  │     ├── file->f_op = &eventpoll_fops
  │     └── file->private_data = ep
  │
  ├── 3. 分配 fd（当前进程的文件描述符表）
  │     └── current->files->fdt[fd] = file
  │
  └── 4. 返回 fd
```

简化数据结构：

```c
// 内核 fs/eventpoll.c（概念级简化）
struct eventpoll {
    wait_queue_head_t     wq;           // epoll_wait 的等待队列
    struct rb_root_cached rbr;          // 红黑树：所有注册的 fd
    struct list_head      rdllist;      // 就绪链表：有事件的 fd
    struct file          *file;         // 关联的文件对象
};

// 红黑树结点（每个注册的 fd 对应一个）
struct epitem {
    struct rb_node        rbn;          // 红黑树节点
    struct list_head      rdllink;      // 就绪链表节点
    struct epoll_filefd   ffd;          // 目标 fd + file 指针
    struct eventpoll     *ep;           // 所属的 epoll 实例
    struct epoll_event    event;        // 用户关注的 events + data
};
```

核心思想：

```
red-black tree（红黑树）
  ┌────────────────────────────────────┐
  │  key = (file_pointer, fd)          │  ← epoll_ctl ADD 时插入
  │  data = epitem → event.events      │
  │         epitem → event.data        │
  │         epitem → rdllink           │
  └────────────────────────────────────┘

ready list（就绪链表）
  ┌──────────────────┐
  │ epitem → rdllink │  ← 有事件的 fd，epoll_wait 直接从这里取
  └──────────────────┘
```

**为什么用红黑树？** —— 快速查找、插入、删除。

```
注册 1 万个 fd:
  epoll_ctl(EPOLL_CTL_ADD): O(log n)   ← 红黑树插入
  epoll_ctl(EPOLL_CTL_DEL): O(log n)   ← 红黑树删除
  epoll_ctl(EPOLL_CTL_MOD): O(log n)   ← 红黑树查找
  epoll_wait:              O(1)         ← 只读就绪链表
```

对比 select：

```
select 注册 1 万个 fd:
  select(): O(n) — 每次把 1 万个 fd 从用户态拷到内核
             然后遍历 1 万个 fd 检查事件
```

**`EPOLL_CLOEXEC`**：执行 `exec()` 新程序时自动关闭这个 fd，防止泄漏给子进程。

**最佳实践：**

```c
int epfd = epoll_create1(EPOLL_CLOEXEC);   // ✅ 推荐
int epfd = epoll_create(256);              // ❌ 旧版 API，size 已废弃
```

---

### 2. `epoll_ctl` — 注册/修改/删除监视的 fd

```c
#include <sys/epoll.h>

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

| 参数 | 含义 |
|------|------|
| `epfd` | `epoll_create1` 返回的 epoll 实例 |
| `op` | 操作类型（见下表） |
| `fd` | 要监视的文件描述符 |
| `event` | 关注的事件 + 用户数据（ADD/MOD 时需要，DEL 时传 NULL） |

**op 取值：**

| 宏 | 含义 | 失败场景 |
|----|------|---------|
| `EPOLL_CTL_ADD` | 把 fd 加入 epoll 监听 | `EEXIST`（已存在） |
| `EPOLL_CTL_MOD` | 修改 fd 的监听事件 | `ENOENT`（不存在） |
| `EPOLL_CTL_DEL` | 从 epoll 移除 fd | `ENOENT`（不存在） |

**`struct epoll_event` 详解：**

```c
struct epoll_event {
    uint32_t     events;   // 位掩码：关注的事件类型
    epoll_data_t data;     // 用户自定义数据（关键字段）
};

typedef union epoll_data {
    void    *ptr;    // libuv 存 uv__io_t 指针
    int      fd;     // 简单场景直接存 fd
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;
```

**events 位掩码（常用）：**

| 宏 | 含义 | 常用搭配 |
|----|------|---------|
| `EPOLLIN` | fd 可读（有数据到达） | ✅ 最常用 |
| `EPOLLOUT` | fd 可写（发送缓冲区空闲） | ✅ 非阻塞写时用 |
| `EPOLLERR` | fd 发生错误（自动监听，可不写） | |
| `EPOLLHUP` | 对端关闭连接 | 检测断开 |
| `EPOLLET` | 边沿触发模式 | ⚠️ 见坑 3 |
| `EPOLLONESHOT` | 事件触发一次后自动移除 | 多线程分发用 |
| `EPOLLRDHUP` | TCP 对端关闭（比 EPOLLHUP 更精确） | 4.1+ |

**三种操作完整示例：**

```c
struct epoll_event ev;

// 添加
ev.events  = EPOLLIN | EPOLLET;
ev.data.ptr = my_watcher;
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

// 修改（关注写事件）
ev.events  = EPOLLIN | EPOLLOUT | EPOLLET;
epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);

// 删除
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
```

---

### 3. `epoll_wait` — 等待事件

```c
#include <sys/epoll.h>

int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);
```

| 参数 | 含义 |
|------|------|
| `epfd` | epoll 实例 |
| `events` | 输出数组，内核把就绪的事件写到这里 |
| `maxevents` | events 数组的大小（> 0） |
| `timeout` | 超时毫秒（-1 = 无限阻塞，0 = 立即返回） |

| 返回值 | 含义 |
|--------|------|
| `> 0` | 就绪的事件个数 |
| `0` | 超时，无事件 |
| `-1` | 失败，errno 指示错误（通常是 `EINTR`） |

**超时值的具体行为：**

```c
epoll_wait(epfd, events, 1024, -1);    // 阻塞直到有事件
epoll_wait(epfd, events, 1024, 0);     // 立即返回（非阻塞轮询）
epoll_wait(epfd, events, 1024, 1000);  // 最多等 1 秒
```

**关键：`events` 数组每次都是新数据**，内核拷贝就绪事件到用户态。不是增量追加。

---

## LT 与 ET 触发模式

### LT（水平触发，Level Triggered — 默认）

```c
// 注册
ev.events = EPOLLIN;             // 不写 EPOLLET == LT
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

// 场景：fd 有 100 字节到达
epoll_wait → 返回 [fd]
read(fd, buf, 50);              // 只读了 50 字节
epoll_wait → 返回 [fd]           // ← 又通知一次！因为还有 50 字节
```

LT 行为：**数据没读完，下次 `epoll_wait` 继续通知**。

### ET（边沿触发，Edge Triggered）

```c
ev.events = EPOLLIN | EPOLLET;   // 加 EPOLLET
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

// 场景：fd 有 100 字节到达
epoll_wait → 返回 [fd]
read(fd, buf, 50);              // 只读了 50 字节
epoll_wait → 阻塞                // ← 不会再通知！
```

ET 行为：**只有状态变化时才通知一次**（从无数据到有数据）。**必须一次把数据读完**，否则剩余数据永远留在内核。

libuv 用 ET 模式，因为它内部自己维护读取缓冲，由 `uv_read_cb` 控制读多少。

---

## 完整最小示例（含错误处理）

```c
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // 1. 创建 epoll 实例
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) { perror("epoll_create1"); exit(1); }

    // 2. 打开串口/管道/socket
    int fd = open("/dev/ttyS0", O_RDONLY | O_NONBLOCK);
    if (fd < 0) { perror("open"); exit(1); }

    // 3. 注册到 epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl");
        exit(1);
    }

    // 4. 事件循环
    struct epoll_event events[1024];
    for (;;) {
        int nfds = epoll_wait(epfd, events, 1024, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;  // 信号中断，重试
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                close(events[i].data.fd);
                continue;
            }

            // ET 模式：必须循环读到 EAGAIN
            char buf[4096];
            int n;
            while ((n = read(events[i].data.fd, buf, sizeof(buf))) > 0) {
                // 处理数据
            }
            if (n < 0 && errno != EAGAIN) {
                // 真实错误
                close(events[i].data.fd);
            }
        }
    }

    close(epfd);
    return 0;
}
```

---

## epoll 常见坑

### 坑 1：ET 模式不读完数据 → 事件丢失

```
epoll_wait 通知有 100 字节 → read 只读 50 → 剩下的 50 永远不会再通知
```

**解法**：ET 模式下必须循环 `read`/`write` 直到返回 `EAGAIN`（无数据了）。

```c
// ❌ 错：读一次就完事
read(fd, buf, sizeof(buf));

// ✅ 对：循环读到 EAGAIN
while ((n = read(fd, buf, sizeof(buf))) > 0) { }
if (n < 0 && errno != EAGAIN) { /* 错误 */ }
```

### 坑 2：`EPOLL_CTL_DEL` 传空 event

```c
// ❌ 错：DEL 传了 ev（没用，但合法，习惯不好）
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);

// ✅ 对：DEL 传 NULL
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
```

### 坑 3：`data.ptr` 指向的内存被释放后事件才到

```c
struct watcher w;
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);  // ev.data.ptr = &w

// 释放 w
close(fd);
// 如果有残留事件，epoll_wait 返回的 data.ptr 指向已释放的内存
// → 野指针，崩溃
```

**解法**：先 `EPOLL_CTL_DEL` 再 `close(fd)`。

### 坑 4：`close(fd)` 不自动从 epoll 移除

`close(fd)` 释放了 fd，但**epoll 内部的红黑树节点不保证立即清理**。如果另一个线程/协程立即重用这个 fd 号，`epoll_wait` 可能把新 fd 的事件误送给你的旧数据。

**解法**：关闭前显式 `EPOLL_CTL_DEL`。

```c
// 正确关闭顺序：
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
close(fd);
```

### 坑 5：`EINTR` 信号中断

```c
// ❌ 错：epoll_wait 被信号中断后直接退出
int nfds = epoll_wait(epfd, events, 1024, -1);
if (nfds < 0) { /* 错误处理 */ }

// ✅ 对：EINTR 重试
int nfds;
do {
    nfds = epoll_wait(epfd, events, 1024, -1);
} while (nfds < 0 && errno == EINTR);
```

libuv 内部会帮你处理 `EINTR`。

### 坑 6：监听同一种 fd 被重复 `EPOLL_CTL_ADD`

```c
// 第一次 ADD → 成功
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);   // 成功
// 第二次 ADD 同一个 fd → EEXIST
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);   // 返回 -1, errno = EEXIST
```

**解法**：第二次用 `EPOLL_CTL_MOD`。

```c
if (已注册) {
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
} else {
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}
```

libuv 的 `uv__io_start` 也是先 `EPOLL_CTL_ADD`，`EEXIST` 时回退到 `EPOLL_CTL_MOD`。

---

## libuv 如何封装 epoll

### 整体架构

```
你的应用代码
  │  egw_transport_open → uv_read_start
  │  uv_run
  ▼
┌──────────────────────────────────────────┐
│           libuv 核心循环                  │
│                                          │
│  uv_run(loop, UV_RUN_DEFAULT)            │
│    └── while (uv__loop_alive(loop))       │
│          ├── uv__io_poll(loop, timeout)  │  ← epoll_wait
│          ├── uv__run_timers(loop)        │  ← 定时器
│          └── uv__run_closing_handles(..) │  ← 关闭回调
└──────────────────────────────────────────┘
  │
  ▼
┌──────────────────────────────────────────┐
│          内核 epoll                      │
│  epoll_wait(backend_fd, events, N, -1)   │  ← 阻塞
│  → 返回就绪事件列表                       │
└──────────────────────────────────────────┘
```

### 核心数据结构

```c
// libuv/include/uv/unix.h（精简）
struct uv__io_s {
    int fd;                         // 监视的 fd（串口、socket 等）
    void (*cb)(uv_loop_t *, uv__io_t *, unsigned int); // 事件回调
    unsigned int events;            // 关注的事件（POLLIN/POLLOUT）
    unsigned int pevents;           // 实际已注册的事件
};

typedef struct uv__io_s uv__io_t;

// libuv/src/unix/internal.h
struct uv__loop_internal_fields_s {
    // ...
};

// libuv/include/uv/unix.h
struct uv_loop_s {
    /* 用户数据 */
    void *data;
    /* 循环控制 */
    unsigned int active_handles;
    void *handle_queue[2];
    union { void *priv; unsigned int time; };
    /* pending */
    int pending_cnt;
    /* 事件轮询 */
    uv__io_t **watchers;          // fd → uv__io_t 映射表（数组）
    unsigned int nwatchers;        // watchers 数组大小
    unsigned int nfds;             // 已使用的 watcher 数量
    int backend_fd;                // epoll_create1 返回的 fd
    /* 各种队列 */
    void *pending_queue[2];
    void *watcher_queue[2];        // 待注册的 watcher 队列
    // ... 更多字段
};
```

**关键设计**：`watchers` 是一个以 fd 为索引的数组：

```c
// loop->watchers[fd] = &uv__io_t;
// 这样 epoll_wait 返回后，通过 events[i].data.fd 直接索引到 uv__io_t：
uv__io_t *w = loop->watchers[events[i].data.fd];
```

### 注册阶段：`uv__io_start` → `epoll_ctl`

调用链（以串口为例）：

```c
// egw_serial_do_open:
uv_read_start((uv_stream_t *)&serial->pipe, on_alloc, on_read);
  │
  ▼
// src/unix/stream.c
int uv_read_start(uv_stream_t *stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
    stream->read_cb = read_cb;              // 存用户回调
    stream->alloc_cb = alloc_cb;

    uv__stream_open(stream, stream->io_watcher.fd, UV_HANDLE_READABLE);
    uv__io_start(stream->loop, &stream->io_watcher, POLLIN);  // ← 关键
    return 0;
}
  │
  ▼
// src/unix/core.c
void uv__io_start(uv_loop_t *loop, uv__io_t *w, unsigned int events) {
    // 1. 更新 watchers 数组
    if (w->fd >= (int)loop->nwatchers) {
        // 扩展数组
    }
    loop->watchers[w->fd] = w;              // fd → uv__io_t 映射
    w->pevents |= events;                    // 合并事件

    // 2. 添加到待注册队列，延迟到 poll 前一起提交
    if (w->events == 0) {                    // 首次注册
        w->events = w->pevents;
        QUEUE_INSERT_TAIL(&loop->watcher_queue, &w->watcher_queue); // 入队
    }
    // 3. 立即插入 epoll（实际上是在 uv__io_poll 前统一处理）
}
  │
  ▼
// 在 uv__io_poll 开始前，libuv 处理 watcher_queue：
// src/unix/core.c
static void uv__io_poll(uv_loop_t *loop, int timeout) {
    // 先处理待注册队列
    while (!QUEUE_EMPTY(&loop->watcher_queue)) {
        w = QUEUE_HEAD(&loop->watcher_queue);
        QUEUE_REMOVE(&w->watcher_queue);

        struct epoll_event e;
        memset(&e, 0, sizeof(e));
        e.events = w->pevents;              // POLLIN | POLLET
        e.data.fd = w->fd;                  // 存 fd 用于索引

        // epoll_ctl：注册到 epoll 红黑树
        if (epoll_ctl(loop->backend_fd, EPOLL_CTL_ADD, w->fd, &e)) {
            if (errno == EEXIST) {           // 已存在则修改
                epoll_ctl(loop->backend_fd, EPOLL_CTL_MOD, w->fd, &e);
            }
        }
    }

    // 然后进入 epoll_wait
    for (;;) {
        nfds = epoll_wait(loop->backend_fd, events, ARRAY_SIZE(events), timeout);
        // ... 处理就绪事件
    }
}
```

### 事件分发阶段：`epoll_wait` → 用户回调

```c
// src/unix/epoll.c — uv__io_poll（核心循环，简化）
void uv__io_poll(uv_loop_t *loop, int timeout) {
    struct epoll_event events[1024];        // 就绪事件数组
    struct kevent *ev;
    int nfds;

    // 处理待注册队列（上面已讲）
    uv__io_poll_process_watcher_queue(loop);

    for (;;) {
        // 阻塞等待
        nfds = epoll_wait(loop->backend_fd, events, ARRAY_SIZE(events), timeout);

        if (nfds == 0) {                     // 超时
            return;
        }

        if (nfds == -1) {
            if (errno == EINTR) {            // 信号中断，重试
                continue;
            }
            if (errno == ENOMEM) {           // 内存不足，重试
                continue;
            }
            return;
        }

        // 处理就绪事件
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;        // 1. 取 fd
            uv__io_t *w = loop->watchers[fd];  // 2. 通过映射表找 uv__io_t

            if (w == NULL) continue;           // 安全防护

            unsigned int revents = events[i].events;

            // libuv 内部标记（非 epoll 标准位）
            if (revents & POLLIN)  revents |= UV__IO_READ;
            if (revents & POLLOUT) revents |= UV__IO_WRITE;

            // 3. 调 uv__io_t 的回调
            w->cb(loop, w, revents);
            //   ↑ 对于串口，这个 cb 是 uv__stream_io
        }
    }
}
```

### stream 层：`uv__stream_io` → 用户 `on_read`

uv__io_t 的 cb 并不是直接指向用户回调，而是 libuv 内部的 stream 处理函数：

```c
// 创建 uv_pipe_t 时，内部设置了：

// src/unix/pipe.c
int uv_pipe_init(uv_loop_t *loop, uv_pipe_t *handle, int ipc) {
    uv__stream_init(loop, (uv_stream_t *)handle, UV_NAMED_PIPE);
    // ...
}

// src/unix/stream.c
void uv__stream_init(uv_loop_t *loop, uv_stream_t *stream, int type) {
    stream->io_watcher.cb = uv__stream_io;     // ← 设为自己的回调
    // ...
}
```

所以事件分发的完整链路是：

```
epoll_wait 返回
  │
  ▼
loop->watchers[fd]  →  uv__io_t（stream->io_watcher）
  │
  ▼
w->cb = uv__stream_io
  │
  ▼
uv__stream_io(loop, &stream->io_watcher, revents)
  │
  ├── 如果是 POLLIN 事件：
  │     uv__read(stream)
  │       ├── read(fd, buf, size)            ← 读串口数据
  │       └── stream->read_cb(stream, buf, len)  ← 调用户 on_data
  │
  └── 如果是 POLLOUT 事件：
        uv__write(stream)
          └── stream->write_cb(stream, status)  ← 调用户 on_write
```

### 完整链路总结（串口示例）

```
egw_serial_do_open                          ← 你的代码
  ├── uv_pipe_init(loop, &pipe, 0)          ← 设置 io_watcher.cb = uv__stream_io
  ├── uv_pipe_open(&pipe, fd)               ← 绑定串口 fd
  └── uv_read_start(stream, alloc_cb, read_cb) ← 设 read_cb = egw_serial_on_read
        └── uv__io_start(loop, &io_watcher, POLLIN)
              └── epoll_ctl(epfd, EPOLL_CTL_ADD, fd, {POLLIN|EPOLLET, .data.fd=fd})

uv_run(loop, UV_RUN_DEFAULT)                ← 启动事件循环
  └── uv__io_poll(loop, timeout)
        └── epoll_wait(epfd, events, 1024, -1)  ← 阻塞等待
              ↓ 串口有数据
        nfds = 1
        events[0].data.fd = serial_fd
        loop->watchers[serial_fd] → uv__io_t
        uv__io_t.cb = uv__stream_io
              ↓
        uv__stream_io(loop, &io_watcher, POLLIN)
              ↓
        uv__read(stream)
              ↓
        read(serial_fd, buf, len)           ← 从内核读到用户态
              ↓
        stream->read_cb = egw_serial_on_read(tp, buf, len)  ← 你的回调
```

---

## 参考

- `man 7 epoll`
- `man 2 epoll_create1`、`man 2 epoll_ctl`、`man 2 epoll_wait`
- 《Unix 网络编程》第 6 章
- libuv 源码：`src/unix/epoll.c`、`src/unix/core.c`（`uv__io_start`、`uv__io_poll`）
- [The Linux Programming Interface] 第 63 章
