# 文件 IO 与管道 — 系统调用学习笔记

## 日期：2026-06-03（第1周）

---

## 一、文件 IO 系统调用

### 1.1 `open()` vs `fopen()`

| | `open()` | `fopen()` |
|---|---|---|
| 标准 | POSIX | ANSI C / C11 |
| 返回 | `int fd`（文件描述符） | `FILE *`（流指针） |
| 缓冲 | 无缓冲（内核态可能缓冲） | 用户态缓冲（FILE 结构体内部） |
| 包含头 | `<fcntl.h>` `<sys/stat.h>` | `<stdio.h>` |
| 可移植 | 仅 POSIX 系统 | 所有 C 环境 |

**`open()` 常用 flag 组合：**

| flags | 含义 |
|---|---|
| `O_RDONLY` | 只读 |
| `O_WRONLY` | 只写 |
| `O_RDWR` | 读写 |
| `O_RDWR \| O_CREAT` | 不存在则创建 |
| `O_RDWR \| O_CREAT \| O_TRUNC` | 创建/截断 |
| `O_WRONLY \| O_CREAT \| O_EXCL` | 排他创建（存在则失败） |
| `O_WRONLY \| O_NONBLOCK` | 非阻塞打开（FIFO 常用） |

**`open()` 原型：**

```c
#include <fcntl.h>
#include <sys/stat.h>

int open(const char *pathname, int flags, mode_t mode);
//                     flags 含 O_CREAT 时需要 mode
//                     失败返回 -1，errno 指示错误
```

**`fopen()` 常用 mode：**

| mode | 含义 |
|---|---|
| `"r"` | 只读 |
| `"w"` | 只写，创建/截断 |
| `"a"` | 追加 |
| `"r+"` | 读写（文件必须存在） |
| `"w+"` | 读写，创建/截断 |
| `"a+"` | 读 + 追加写 |

**⚠ 注意事项：**

- `open()` + `O_CREAT` 时 **必须提供第三个参数 mode**（如 `0644`），否则权限随机
- `fopen()` mode 严格区分 `"w"`（截断）和 `"a"`（追加），选错会丢数据
- `fopen()` 是标准 C 库函数，跨平台首选；`open()` 是系统调用，有更细粒度控制
- 二进制数据用 `"wb"` / `"rb"`，否则 Windows 下 `\n` → `\r\n` 自动转换（Linux 无区别）

### 1.2 `close()` / `fclose()`

```c
#include <unistd.h>
int close(int fd);          // POSIX

#include <stdio.h>
int fclose(FILE *stream);   // C11
```

**⚠ 注意事项：**

- **必须检查 `close()` 返回值** —— 某些文件系统（如 NFS）将写错误延迟到 `close()` 时才返回
- **不要 double close** —— 第二次 `close(fd)` 行为未定义（可能恰好关掉另一个线程新开的 fd）
- `fclose()` 自动 flush 缓冲区后再 close；用 `close()` 关 `fileno(fp)` 会导致缓冲数据丢失
- 典型安全写法：

```c
if (close(fd) == -1) {
    perror("close");
    // 处理错误（如日志记录，但不重试）
}
fd = -1;  // 防止误用
```

### 1.3 `lseek()` / `fseek()`

```c
#include <unistd.h>
off_t lseek(int fd, off_t offset, int whence);
// 失败返回 -1

#include <stdio.h>
int fseek(FILE *stream, long offset, int whence);
// 成功返回 0，失败返回非 0
```

**whence 取值：**

| whence | 参考点 |
|---|---|
| `SEEK_SET` | 文件开头 |
| `SEEK_CUR` | 当前位置 |
| `SEEK_END` | 文件末尾 |

**⚠ 注意事项：**

- **管道、FIFO、socket 不支持 lseek** —— 返回 `-1`，errno = `ESPIPE`
- `lseek()` 允许移动超过文件末尾（创建空洞，不占用磁盘块）
- `fseek()` 的 `offset` 是 `long` 类型，大文件（>2GB）需用 `fseeko()` / `ftello()` 或 64 位 API
- 读写模式切换时（如 `fwrite` 后立刻 `fread`），需先 `fseek()` / `fflush()`

### 1.4 `read()` / `write()` vs `fread()` / `fwrite()`

```c
// POSIX 无缓冲
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

// C11 缓冲
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
```

**⚠ 关键区别：**

| | `read/write` | `fread/fwrite` |
|---|---|---|
| 缓冲 | 无（直接系统调用） | 用户态缓冲 |
| 返回 | 实际读写字节数（可 < count） | 成功读写的"元素"个数 |
| 部分读 | 常见（尤其管道、socket） | 少见 |
| 信号中断 | `EINTR`（需重启） | 库内部处理 |
| 适用场景 | 管道、FIFO、socket | 普通文件、结构化数据 |

**⚠ 注意事项：**

- `read()` **不一定返回请求的全部字节** —— 管道、socket、信号中断时返回更少。需要循环读取：

```c
ssize_t read_full(int fd, void *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t n = read(fd, (char *)buf + total, count - total);
        if (n == -1) {
            if (errno == EINTR) continue;  // 信号中断，重试
            return -1;
        }
        if (n == 0) break;  // EOF
        total += (size_t)n;
    }
    return (ssize_t)total;
}
```

- `fread()` 与 `fwrite()` 的返回值是"完整元素数"，非字节数。检查错误需：

```c
size_t n = fread(buf, 1, 100, fp);
if (n == 0 && ferror(fp)) { /* 发生错误 */ }
```

### 1.5 `mkfifo()` — 创建命名管道

```c
#include <sys/types.h>
#include <sys/stat.h>

int mkfifo(const char *pathname, mode_t mode);
// 成功返回 0，失败返回 -1
```

**⚠ 注意事项：**

- `mkfifo()` 只创建文件系统节点（inode），**不打开文件**。后续仍需 `open()`
- 文件已存在时返回 -1，errno = `EEXIST`
- `mode` 受 umask 影响 —— 传 `0666` 实际权限为 `0666 & ~umask`
- 创建的 FIFO 用 `unlink()` / `remove()` 删除
- 对比：
  - 匿名管道（`pipe()`）：只能在有亲缘关系的进程间使用
  - 命名管道（`mkfifo()`）：任意进程可通过文件系统路径访问

---

## 二、匿名管道（pipe）

### 2.1 基本使用

```c
#include <unistd.h>

int pipe(int pipefd[2]);
// pipefd[0] = 读端, pipefd[1] = 写端
// 成功返回 0，失败返回 -1
```

**典型 fork + pipe 模式：**

```c
int fds[2];
pipe(fds);

pid_t pid = fork();

if (pid == 0) {
    // 子进程：关写端，读
    close(fds[1]);
    read(fds[0], buf, sizeof(buf));
    close(fds[0]);
} else {
    // 父进程：关读端，写
    close(fds[0]);
    write(fds[1], data, len);
    close(fds[1]);
    wait(NULL);
}
```

**⚠ 注意事项：**

- **必须关闭不需要的端** —— 读写共用一个 pipe，不用的一端不关会导致：
  - 写端不关 → 读端 read() 不会返回 EOF（因为仍有写端打开的可能）
  - 读端不关 → 写端写满缓冲区后阻塞
- 管道缓冲区大小：Linux 通常 65536 字节（可 `fcntl(fd, F_SETPIPE_SZ)` 调整）
- **原子写** —— 单次写入 ≤ `PIPE_BUF`（POSIX 要求 ≥ 512，Linux 通常 4096）保证不与其他进程交错
- 写 > `PIPE_BUF` 字节时，写入可能与其他写者交错

### 2.2 pipe 边缘情况

| 场景 | 行为 |
|---|---|
| 读端已关，写端写入 | `write()` → 返回 -1，errno = `EPIPE`；同时收到 `SIGPIPE` |
| 写端已关，读端读取 | `read()` → 返回 0（EOF 标志） |
| 缓冲区满，写端写入 | `write()` → 阻塞（或 `O_NONBLOCK` 返回 `EAGAIN`） |
| 缓冲区空，读端读取 | `read()` → 阻塞（或 `O_NONBLOCK` 返回 `EAGAIN`） |
| 收到信号中断 | `read()` / `write()` → 返回 -1，errno = `EINTR` |

### 2.3 SIGPIPE 处理

写已关读端的管道时：
1. 内核向进程发送 `SIGPIPE` 信号
2. 默认行为是终止进程
3. 需显式处理才能继续运行：

```c
// 方式一：忽略信号（推荐场景：确定不需要处理）
signal(SIGPIPE, SIG_IGN);

// 方式二：自定义处理（推荐场景：需要记录日志）
void sigpipe_handler(int sig) {
    (void)sig;
    fprintf(stderr, "SIGPIPE: pipe broken\n");
}
struct sigaction sa;
memset(&sa, 0, sizeof(sa));
sa.sa_handler = sigpipe_handler;
sigaction(SIGPIPE, &sa, NULL);
```

---

## 三、命名管道（FIFO）

### 3.1 生命周期

```
mkfifo()     →   open()     →   read()/write()     →   close()     →   unlink()
创建节点         打开连接         数据传输               关闭             删除
```

### 3.2 open() 阻塞行为

FIFO 的 `open()` 有两种模式，这是与普通文件最大的区别：

| 打开方式 | 无对应端时 |
|---|---|
| `open(path, O_RDONLY)` | 阻塞直到有写端 `open()` |
| `open(path, O_WRONLY)` | 阻塞直到有读端 `open()` |
| `open(path, O_RDONLY \| O_NONBLOCK)` | 立即返回（若无写端，`read()` 返回 0） |
| `open(path, O_WRONLY \| O_NONBLOCK)` | 无读端时返回 -1，errno = `ENXIO` |

**⚠ 关键陷阱：**

- 只写打开（如 simulator 当前做法）默认 **阻塞** —— 必须等到另一个进程以只读打开才返回
- 阻塞设计是特性不是 bug —— 它自动实现了"生产者等待消费者"
- 如果希望非阻塞的行为，需加 `O_NONBLOCK` 或使用读写双向打开（`O_RDWR` 绕过阻塞检查，但语义不推荐）

### 3.3 与匿名管道的对比

| 对比维度 | 匿名管道 (pipe) | 命名管道 (FIFO) |
|---|---|---|
| 创建 | `pipe()` | `mkfifo()` + `open()` |
| 标识 | 无（仅 fd 对） | 文件系统路径 |
| 进程关系 | 需亲缘（fork 继承 fd） | 任意进程 |
| 打开阻塞 | 无（直接创建） | 有（需匹配读/写端） |
| 数据传输 | 字节流 | 字节流 |
| lseek | 不支持（ESPIPE） | 不支持（ESPIPE） |
| 关闭 | `close(fd)` | `close(fd)` + `unlink(path)` |
| 清除 | 自动（fd 关闭即销毁） | 手动 `unlink()` |

### 3.4 读写语义（与匿名管道完全一致）

- 没有 `lseek` —— 字节流，一次性读取
- `read()` 返回 0 表示写端已关闭
- 写端关闭后重建：新写端 `open()` 后，读端 `read()` 重新开始阻塞？

**
 <answer>等待新数据</answer>*
- 半双工 —— 同一时间只能单向传输

---

## 四、初学者常见误区

### 4.1 文件描述符的观念

**文件描述符（fd）就是一个整数** —— 0=stdin, 1=stdout, 2=stderr。第一次 `open()` 返回 3，第二次返回 4，依次递增。`close(3)` 后内核复用这个数字。

```c
close(0);          // 关掉 stdin
int fd = open("xxx", O_RDONLY);  // fd == 0！因为内核复用最小的空闲号
```

这就是为什么重定向（shell 的 `<` `>`）的原理 —— 先 `close(0)` 再 `open` 文件，文件自动变成 stdin。

### 4.2 FILE* 和 fd 的区别（最容易混淆）

| | `FILE*`（流） | `int fd`（描述符） |
|---|---|---|
| 本质 | 结构体指针，含缓冲区 | 内核数组的索引 |
| 读写 | 先到缓冲区，满/刷新时才进内核 | 直接进内核 |
| 函数 | `fread()` `fwrite()` `fprintf()` | `read()` `write()` |
| 性能 | 频繁小数据读写快 | 大数据块较快 |
| 混用 | ❌ 不要混用 —— 缓冲区会导致数据乱序 | — |

**⚠ 混用 FILE* 和 fd 的典型错误：**

```c
// 错误！fprintf 把数据放到了用户态缓冲区，write 直接绕过它
FILE *fp = fopen("log.txt", "w");
fprintf(fp, "hello ");    // 在缓冲区里，还没写进内核
int fd = fileno(fp);
write(fd, "world\n", 6);  // 直接写进内核，跑到 hello 前面

// 输出: "world\nhello "  而不是 "hello world\n"
```

**正确做法：** 只用一套 API，或用 `fflush(fp)` 刷新后再用 `write`。

### 4.3 缓冲区未 flush 导致的问题

```c
FILE *fp = fopen("out.txt", "w");
fprintf(fp, "data");   // 在缓冲区
// 程序在这里崩溃 —— 文件是空的！
fclose(fp);            // fclose 会 flush，但没执行到
```

**解决方法：**

- 关键数据写完后调用 `fflush(fp)` 强制写入
- 使用 `setbuf(fp, NULL)` 关闭缓冲（代价：性能下降）
- 需要即时写入的场景用 `write()` 代替 `fprintf()`

### 4.4 open() + O_CREAT 忘传 mode

```c
int fd = open("new.txt", O_CREAT | O_RDWR);
// 第三个参数 mode 没传！new.txt 权限随机（如 000）
```

**结果：** 文件权限不可控，可能无法读写。

**正确：**

```c
int fd = open("new.txt", O_CREAT | O_RDWR, 0644);
// 0644 = rw-r--r--（考虑了 umask）
```

### 4.5 read() 没返回值或返回值处理不当

```c
// 错误：假设 read 一次能读完所有数据
char buf[1024];
read(fd, buf, 1024);    // 可能只读了 10 字节
buf[1023] = '\0';        // 后 1013 字节是垃圾数据
printf("%s\n", buf);     // 输出乱码
```

**纠正：**

```c
ssize_t n = read(fd, buf, sizeof(buf) - 1);
if (n > 0) {
    buf[n] = '\0';       // 只处理实际读到的字节
    printf("%s\n", buf);
}
```

### 4.6 管道读写不关闭无用端

```c
int fds[2];
pipe(fds);
fork();

if (/* 子进程 */) {
    // 子进程只读
    char buf[64];
    read(fds[0], buf, 64);     // OK
    // 但是没关 fds[1]！父进程关写端后，子进程还开着写端
    // read() 永远不会返回 0
}

if (/* 父进程 */) {
    write(fds[1], "hi", 2);
    close(fds[1]);              // 关写端
    // 但没关 fds[0]！子进程读端 open 计数不为 0
}
```

**后果：** `read()` 永远不会返回 EOF（0），因为内核认为还有写端打开。

**原则：** fork 后，**不需要的端立刻关掉**。

### 4.7 SIGPIPE 信号杀死进程

```c
// 最简单的管道写入
write(pipe_fd, data, len);
// 如果读端已关闭 → 进程被 SIGPIPE 杀死（默认行为）
// 没有错误信息，没有 core dump，就是"不见了"
```

**解决方案：** 在看门狗 / 持续运行的进程中，必须处理 SIGPIPE。

### 4.8 收到 EINTR 时不重试

```c
ssize_t n = read(fd, buf, 100);
// 如果进程在 read 期间收到信号（如 SIGALRM）
// read 返回 -1，errno = EINTR
// 数据没读成，但这不是"错误"——就是被中断了
```

**正确做法：**

```c
ssize_t n;
do {
    n = read(fd, buf, 100);
} while (n == -1 && errno == EINTR);
```

大部分系统编程"奇怪的问题"最终都是这几个原因之一。记住：**检查每个系统调用的返回值，永远不要假设一次读写能完成全部数据**。

---

## 五、调试技巧

详细调试指南见独立文档 `journal/debugging-guide.md`，包含：

- `strace` 追踪系统调用（过滤、统计、解读输出）
- `gdb` 调试 fork 程序、core dump 分析
- `valgrind` 内存泄漏检测
- AddressSanitizer 集成
- FIFO 专用调试（手动模拟、状态检查、残留清理）
- errno 完整参考（编号、场景、排查方向）
- 常见调试场景速查

---

## 六、M1 第 1 周项目中的应用

### 数据流架构验证结果

```
data_gen → [匿名管道] → fifo_writer → [/tmp/temp_fifo] → cat
(子进程)                   (父进程)                          (外部)
```

验证输出：
```
$ cat /tmp/temp_fifo
58.5
80.3
33.5
75.4
```

SIGPIPE 断开时父进程不崩溃，打印警告后继续尝试写入。

---

## 参考资源

- 《Unix/Linux 系统编程手册》第 4-5 章（文件 IO）、第 44 章（管道和 FIFO）
- `man 2 open`, `man 2 pipe`, `man 3 mkfifo`, `man 7 pipe`, `man 7 signal`
- `man 3 fopen`, `man 3 fread`, `man 3 fseek`
