# eventfd 学习笔记

## 是什么

eventfd 是 Linux 2.6.22+ 引入的一个**轻量级内核事件通知对象**。本质上是一个由内核维护的 **64 位无符号计数器**，通过 fd 暴露给用户态。

```
用户态看到的：一个 fd
                     ▼
内核态维护的：一个 uint64_t 计数器（+ 等待队列）
```

你不需要像 pipe 那样分配缓冲区，不需要像信号量那样调 sem_*，不需要像 socketpair 那样走完整的网络栈。eventfd 就是"一个数字 + 能等它变化"。

---

## API 一览

### 创建

```c
#include <sys/eventfd.h>

int efd = eventfd(unsigned int initval, int flags);
```

| flags | 含义 |
|-------|------|
| `0` | 阻塞 read/write |
| `EFD_NONBLOCK` | 非阻塞 |
| `EFD_CLOEXEC` | `exec()` 时自动关闭 |
| `EFD_SEMAPHORE` | 信号量模式（见下文） |

返回的 fd 可以正常使用 `read`/`write`/`poll`/`select`/`epoll`。

### 写（通知）

```c
uint64_t val = 1;
write(efd, &val, sizeof(uint64_t));
```

内核将计数器的**当前值**加上 `val`（原子操作）。如果结果超过 `UINT64_MAX - 1`，write 阻塞或返回 `EAGAIN`。

### 读（消费）

```c
uint64_t val;
read(efd, &val, sizeof(uint64_t));
```

返回计数器的**当前值**并将计数器清零（普通模式），或减 1（信号量模式）。如果计数器为 0，read 阻塞或返回 `EAGAIN`。

### poll / epoll

eventfd 可读（计数器 > 0）或可写（计数器未溢出）时，通过 epoll 正常唤醒。

---

## 两种模式

### 普通模式（默认）

```
                write(1) → 计数器 += 1
                write(3) → 计数器 += 3

read() → 返回计数器的当前值并清零

场景：
  - 通知"来活了"，计数器值不重要
  - 多个写者合并通知（写 10 次 read 1 次就消费了 10 个信号）
```

### 信号量模式（`EFD_SEMAPHORE`）

```
                write(1) → 计数器 += 1

read() → 返回 1，计数器 -= 1（不是清零！）

                write(3) → 计数器 += 3

read() → 返回 1，计数器 -= 1
read() → 返回 1，计数器 -= 1
read() → 返回 1，计数器 -= 1

场景：
  - 需要精确匹配"写了几次就读几次"
  - 类似信号量的 P/V 操作
```

---

## 内核实现（简化）

### 核心结构体

```c
// fs/eventfd.c
struct eventfd_ctx {
    __u64 count;                    // 64 位计数器
    unsigned int flags;             // EFD_SEMAPHORE | EFD_NONBLOCK
    wait_queue_head_t wqh;          // 等待队列（poll/read 阻塞者挂在这）
};
```

### write 操作

```c
ssize_t eventfd_write(struct file *file, const char __user *buf, size_t count) {
    struct eventfd_ctx *ctx = file->private_data;
    __u64 ucnt;
    
    copy_from_user(&ucnt, buf, sizeof(ucnt));  // 从用户态拷贝 8 字节
    
    spin_lock_irq(&ctx->wqh.lock);

    // 检查溢出
    if (ucnt > 0 && ctx->count > (U64_MAX - ucnt - 1)) {
        spin_unlock_irq(&ctx->wqh.lock);
        return /* EAGAIN 或阻塞 */;
    }

    ctx->count += ucnt;                         // 原子加（在锁保护下）

    // 唤醒所有等待此 eventfd 的线程（epoll/poll/read）
    wake_up_locked_poll(&ctx->wqh, EPOLLIN);

    spin_unlock_irq(&ctx->wqh.lock);
    return sizeof(ucnt);
}
```

### read 操作

```c
ssize_t eventfd_read(struct file *file, char __user *buf, size_t count) {
    struct eventfd_ctx *ctx = file->private_data;

    spin_lock_irq(&ctx->wqh.lock);

    if (ctx->count == 0) {
        spin_unlock_irq(&ctx->wqh.lock);
        return /* EAGAIN 或阻塞直到 count > 0 */;
    }

    if (ctx->flags & EFD_SEMAPHORE) {
        ucnt = 1;
        ctx->count--;               // 减 1
    } else {
        ucnt = ctx->count;
        ctx->count = 0;             // 清零
    }

    spin_unlock_irq(&ctx->wqh.lock);

    copy_to_user(buf, &ucnt, sizeof(ucnt));
    return sizeof(ucnt);
}
```

---

## 为什么 libuv 选 eventfd 而不是 pipe？

| | pipe | eventfd |
|---|---|---|
| 内核对象大小 | 至少 4096 字节环形缓冲区 | 8 字节计数器 |
| write 系统调用 | 拷贝数据 → 可能触发软中断 | 一次原子加，更快 |
| 通知后消费 | 必须 `read` 出数据清空缓冲区 | 不需要读，内核维护计数器 |
| 多线程竞争 | 多个写者可能交错写入，需要读端解析 | 原子加，天然合并 |
| fd 数量 | 2 个（读端 + 写端） | 1 个 |

**关键优势：不需要消费。** libuv 的 `uv__async_io` 在 Linux 上不读 eventfd。多个线程同时 `uv_async_send`，eventfd 计数器累加但不溢出，loop 线程醒来一次遍历所有 async handle 即可。用 pipe 的话，loop 线程必须 `read` 消费数据，否则 pipe 缓冲区满后写端会阻塞。

---

## 在你的项目中的角色

```
            ┌──────────────────┐
主线程      │  egw_task_post   │
            │    → push task   │
            │    → eventfd +1  │
            └────────┬─────────┘
                     │
                     ▼ 内核 eventfd 计数器
                     │
            ┌────────┴─────────┐
loop 线程   │  epoll_wait 返回  │
            │  → uv__async_io  │
            │  → on_task       │
            │  → pop + 执行    │
            └──────────────────┘
```

eventfd 只传递"来活了"这一个比特的信息。实际的 task 数据（close 哪个 tp、timer 参数等）通过共享内存的 `task_queue` + `task_lock` 传递。eventfd 不需要携带内容，所以不需要读出来消费——这正是它比 pipe 更适合的地方。

---

---

## Linux eventfd vs RTOS 任务通知

以 FreeRTOS 的 `xTaskNotifyGive` / `ulTaskNotifyTake` 为例做对比。

### 核心差异

| | Linux eventfd | RTOS 任务通知 (FreeRTOS) |
|---|---|---|
| **本质** | 内核文件对象，通过 fd 操作 | Task Control Block 中的一个 `ulNotifiedValue` 字段 |
| **操作方式** | `read` / `write` 系统调用 | `xTaskNotifyGive()` / `ulTaskNotifyTake()` 函数调用 |
| **阻塞机制** | `poll` / `epoll` / 阻塞 `read` | 调用 `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` 时任务挂起 |
| **上下文限制** | 任何线程 / 进程 / 信号处理函数都可写 | 只能在 RTOS 任务或中断中调用 |
| **跨进程** | ✅ 可通过 UNIX domain socket 传递 fd | ❌ 不适用（没有进程概念） |
| **多个接收者** | 多个线程可同时 `poll` 同一个 eventfd | 一个通知只能唤醒一个任务 |
| **通知合并** | 多个 `write` 合并为一次计数器累加 | `xTaskNotifyGive` 也是累加，`ulTaskNotifyTake` 减 1（和信号量模式一致） |
| **自定义数据** | 写入的 8 字节值可以编码信息 | `xTaskNotify` 可以传 32 位值或直接覆盖/置位/递增 |
| **超时控制** | `poll` 的 timeout 参数 | `ulTaskNotifyTake` 的 `xTicksToWait` |

### 简单对比图

```
Linux:                          RTOS:
  eventfd (计数器)                TCB.ulNotifiedValue
  │                                │
  ├── write(1)      ───────      ├── xTaskNotifyGive()
  ├── read → 返回值   ───────      ├── ulTaskNotifyTake() → 返回值
  ├── poll/epoll     ───────      ├── 任务阻塞在 ulTaskNotifyTake()
  └── 进程/线程/中断都可写         └── 任务/中断中可调用
```

### 本质区别：设计哲学

**eventfd** 是一个**内核对象**，不绑定到任何调用者。创建它的进程、其他进程、任何线程都可以写它。读走通知的线程也不需要是创建者。它是一个"信箱"，谁投信、谁收信无所谓。

**RTOS 任务通知** 是**绑定到具体任务**的。`xTaskNotifyGive(task_handle)` 需要知道目标任务的句柄。通知是直接写到那个任务的 TCB 里的——"你有一条消息"。它是"点对点的电话"。

### 如果你在 RTOS 上实现 `uv_async_send`

RTOS 没有 eventfd，但 libuv 在 RTOS 上的移植通常的做法是：

```
uv_async_send(loop)
  → 往 loop 线程的任务通知发一个通知
  → loop 线程不是在 epoll_wait 上
    而是在一个 while(1) 循环里调 ulTaskNotifyTake 等通知
```

RTOS 版本的 libuv 的 "poll" 阶段不是 `epoll_wait`，而是 `ulTaskNotifyTake` + 检查所有注册的 fd（用 `select` 模拟或直接用硬件中断）。

在你的项目里如果你移植到 RTOS 上跑，`uv_async_send` 底层会变成 `xTaskNotifyGive(loop_task_handle)`，`notify_fd`（eventfd）也会换成另一个任务通知或一个队列。

---

## 参考

- `man 2 eventfd`
- `man 7 eventfd`
- Linux 源码：`fs/eventfd.c`
- libuv 源码：`src/unix/async.c`（`uv__async_send` + `uv__async_start`）
