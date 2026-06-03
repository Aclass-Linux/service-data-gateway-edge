# Linux 系统编程调试指南

## 适用场景

调试 M1 第 1 周的 simulator 及后续网关程序。所有命令基于 Linux，假定使用 GCC/Clang 工具链。

---

## 一、strace — 追踪系统调用

### 1.1 基本用法

```bash
# 追踪一个新启动的进程
strace ./build/src/app/simulator

# 追踪已运行的进程
strace -p <PID>

# 同时追踪子进程（fork 必需）
strace -f ./build/src/app/simulator
```

### 1.2 过滤输出

```bash
# 只显示特定系统调用
strace -e trace=read,write,open,openat,close ./build/src/app/simulator

# 按类别过滤
strace -e trace=%file      # 文件相关（open, read, write, close...）
strace -e trace=%ipc       # IPC 相关（pipe, shmget, semop...）
strace -e trace=%process   # 进程相关（fork, exec, exit...）
strace -e trace=%signal    # 信号相关（kill, sigaction...）
strace -e trace=%network   # 网络相关（socket, connect...）

# 排除某些调用（减少噪音）
strace -e trace=!mmap,brk  # 不显示内存映射调用
```

### 1.3 输出格式控制

```bash
# 显示时间戳（从启动开始）
strace -t ./build/src/app/simulator

# 显示精确时间差
strace -r ./build/src/app/simulator

# 显示当前时间
strace -T ./build/src/app/simulator     # 显示每个调用的耗时
strace -tt ./build/src/app/simulator    # 微秒级时间戳

# 不截断字符串
strace -s 1024 ./build/src/app/simulator  # 默认 32 字节
```

### 1.4 统计模式

```bash
# 统计每个系统调用的次数、耗时
strace -c ./build/src/app/simulator

# 输出示例：
# % time     seconds  usecs/call     calls    errors  syscall
# ------ ----------- ----------- --------- --------- ----------------
#  45.23    0.023456        2345        10         0  write
#  30.12    0.015678        1567        10         0  read
#  12.34    0.006789         678        10         0  openat
```

### 1.5 实用场景示例

```bash
# 场景：simulator 启动后卡住不动
strace -f -e trace=open,openat,read,write ./build/src/app/simulator
# 输出会显示 open("/tmp/temp_fifo", O_WRONLY) 卡住 —— 说明在等读端

# 场景：进程突然退出，不知原因
strace -f ./build/src/app/simulator
# 如果是 SIGPIPE，会看到:
# --- SIGPIPE {si_signo=SIGPIPE, ...} ---
# +++ killed by SIGPIPE +++

# 场景：检查 FIFO 是否正确创建
strace -e trace=mkfifo,stat ./build/src/app/simulator
```

### 1.6 解读输出

```
openat(AT_FDCWD, "/tmp/temp_fifo", O_WRONLY) = 4
├── 调用名        └── 路径              └── 标志     └── 返回值
│                                                       (fd=4)
└── 参数
```

- 返回值 = 4：成功，新 fd 是 4
- 返回值 = -1：失败，用 `errno` 判断原因
- 调用名后缀 `at`（如 `openat`）：现代 Linux 内核版本，语义同 `open`/`stat`

---

## 二、gdb — 调试器

### 2.1 启动与基础

```bash
# 启动调试
gdb ./build/src/app/simulator

# 带参数启动
gdb --args ./build/src/app/simulator --daemon

# 调试 core dump
gdb ./build/src/app/simulator core

# 附加到运行中的进程
gdb -p <PID>
```

### 2.2 断点管理

```gdb
# 按文件名 + 行号
b simulator.c:42

# 按函数名
b fifo_writer_init
b main

# 条件断点
b data_gen.c:10 if temp > 90.0

# 临时断点（触发一次后自动删除）
tb fifo_writer.c:30

# 列出所有断点
info b

# 删除断点
d 1        # 删除编号为 1 的断点
d          # 删除所有断点

# 启用/禁用
disable 2
enable 2
```

### 2.3 运行控制

```gdb
r              # run — 运行程序
c              # continue — 继续执行
n              # next — 单步跳过（不进入函数）
s              # step — 单步进入函数
finish         # 执行到当前函数返回
until 55       # 执行到第 55 行
ctrl+c         # 中断运行中的程序（在 gdb 内）
```

### 2.4 查看数据

```gdb
p temp         # print — 打印变量值
p &temp        # 打印变量地址
p/x temp       # 十六进制显示
p/f temp       # 浮点数显示

display temp   # 每次停顿时自动打印 temp
undisplay 1    # 取消自动打印
```

### 2.5 调试 fork 程序

```gdb
# 方法 1：启动时设置
set follow-fork-mode child    # gdb 跟踪子进程
set follow-fork-mode parent   # gdb 跟踪父进程（默认）

# 方法 2：运行时动态切换
catch fork                    # fork 时暂停，然后选择
# 程序停在 fork 处后:
set follow-fork-mode child
c

# 查看当前跟踪哪个进程
info inferiors
# 切换
inferior 2
```

### 2.6 调试已崩溃的程序（core dump）

```bash
# 1. 允许生成 core dump
ulimit -c unlimited

# 2. 运行程序直到崩溃
./build/src/app/simulator

# 3. 调试 core 文件
gdb ./build/src/app/simulator core

# 4. 查看崩溃位置
bt                 # backtrace — 调用栈
frame 3            # 切换到栈帧 3
info locals        # 查看局部变量
```

**⚠ 常见问题：core dump 没有生成**

```bash
# 检查限制
ulimit -c          # 输出 0 表示被禁止

# 检查 core 文件路径
cat /proc/sys/kernel/core_pattern
# 可能输出:
# |/usr/share/apport/apport %p %s %c %d %P   (Ubuntu 默认管道到 apport)
# 或 core       (当前目录下生成 core.xxx)
# 或 /var/crash/core.%p  (固定路径)

# 临时生效
ulimit -c unlimited
```

### 2.7 实用 gdb 技巧

```gdb
# 打印 errno 信息
p errno
call perror("")
p strerror(errno)

# 查看源码
list           # 列出当前位置附近 10 行
list 42,60     # 列出 42-60 行
list fifo_writer.c:10,30

# 修改变量值（测试不同分支）
set temp = 99.9

# 调用函数
call fifo_writer_cleanup()

# 查看内存
x/10xb buf     # 以十六进制显示 buf 的 10 字节
x/10s buf      # 以字符串显示 buf
```

---

## 三、valgrind — 内存检查

### 3.1 内存泄漏检测

```bash
# 基本用法
valgrind ./build/src/app/simulator

# 详细输出
valgrind --leak-check=full --show-leak-kinds=all ./build/src/app/simulator

# 输出到文件
valgrind --log-file=valgrind.log ./build/src/app/simulator
```

### 3.2 解读 valgrind 输出

```
==12345== 40 bytes in 1 blocks are definitely lost in loss record 1 of 1
==12345==    at 0x484...

```

**级别：**
| 级别 | 含义 | 优先级 |
|---|---|---|
| `definitely lost` | 确定泄漏（malloc/free 不配对） | **必须修** |
| `indirectly lost` | 间接泄漏（容器丢失导致内部泄漏） | 必须修 |
| `possibly lost` | 可能泄漏（指针被覆盖） | 建议修 |
| `still reachable` | 退出时未释放（全局变量） | 通常可忽略 |

### 3.3 与 simulator 配合

```bash
# simulator 需要外部读端才能退出
# 用 timeout 配合
timeout 5 valgrind --leak-check=full ./build/src/app/simulator
# 或另一个终端 cat /tmp/temp_fifo 后 kill 父进程
```

---

## 四、AddressSanitizer（ASan）

比 valgrind 更快，编译时开启：

```bash
# 编译时加 flag
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
cmake --build build

# 运行（自动检查）
./build/src/app/simulator

# 发现错误时会打印详细的堆栈信息：
# ==ERROR: AddressSanitizer: heap-buffer-overflow on address ...
```

**CMake 集成（添加到顶层 CMakeLists.txt）：**

```cmake
# 在 Debug 模式下开启 ASan
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
```

---

## 五、printf 调试法

虽然原始，但在管道/FIFO 场景中经常是最快的。

### 5.1 打印到 stderr（推荐，不影响管道数据）

```c
fprintf(stderr, "DEBUG: temp=%.1f, pid=%d\n", temp, getpid());
```

### 5.2 日志级别宏

```c
#define DEBUG_LOG(fmt, ...) \
    fprintf(stderr, "[DEBUG %s:%d] " fmt "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

// 使用
DEBUG_LOG("temp=%.1f", temp);
// 输出: [DEBUG simulator.c:42] temp=58.5
```

### 5.3 注意缓冲区

```c
// stderr 默认无缓冲，stdout 是行缓冲
// 如果必须用 stdout:
fprintf(stdout, "data\n");
fflush(stdout);    // 强制刷出，否则管道里可能看不到
```

---

## 六、FIFO 专用调试技巧

### 6.1 手动模拟读写端

```bash
# 终端 1：启动 simulator（它会阻塞在 open）
./build/src/app/simulator
# 输出: Waiting for FIFO reader on /tmp/temp_fifo ...

# 终端 2：连接读端
cat /tmp/temp_fifo
# simulator 立即解除阻塞，开始输出数据
```

### 6.2 检查 FIFO 状态

```bash
# 查看 FIFO 属性
stat /tmp/temp_fifo
# 文件类型: 命名管道 (FIFO)

# 查看哪些进程打开了 FIFO
lsof /tmp/temp_fifo
# 或
fuser -v /tmp/temp_fifo

# 查看管道缓冲区有多少数据
# 没有直接命令，但可以用 strace 追踪 read/write 字节数
```

### 6.3 FIFO 残留清理

```bash
# 如果程序异常退出，FIFO 文件可能残留
ls -la /tmp/temp_fifo

# 手动删除
rm /tmp/temp_fifo

# 或检查残留
test -p /tmp/temp_fifo && rm /tmp/temp_fifo
```

### 6.4 超时控制

```bash
# 用 timeout 防止程序永远阻塞
timeout 5 ./build/src/app/simulator
# 5 秒后自动发 SIGTERM

# 配合 valgrind
timeout 5 valgrind ./build/src/app/simulator
```

---

## 七、errno 完整参考

### 7.1 查看 errno 的值

```bash
# 查看数字对应的名称
errno -l | grep 32

# 查看错误描述
errno EPIPE
# => EPIPE 32 Broken pipe
```

### 7.2 代码中打印 errno

```c
#include <string.h>
#include <errno.h>

int fd = open("/tmp/test", O_RDONLY);
if (fd == -1) {
    // 方式 1：perror（最简单）
    perror("open");

    // 方式 2：strerror（灵活）
    fprintf(stderr, "open failed: %s\n", strerror(errno));

    // 方式 3：同时打印 errno 数字
    fprintf(stderr, "open failed (errno=%d): %s\n",
            errno, strerror(errno));
}
```

### 7.3 本次项目常见 errno

| errno 数字 | 名称 | 典型场景 | 排查方向 |
|---|---|---|---|
| 1 | `EPERM` | 操作不允许 | 检查权限 |
| 2 | `ENOENT` | 文件或路径不存在 | `mkfifo` 前父目录存在吗？ |
| 4 | `EINTR` | 系统调用被信号中断 | 加重试循环 |
| 9 | `EBADF` | 无效文件描述符 | fd 是否已 close 未检查？ |
| 11 | `EAGAIN` | 资源暂不可用 | `O_NONBLOCK` 下管道空/满 |
| 13 | `EACCES` | 权限不足 | 文件 644? FIFO 0666? umask? |
| 17 | `EEXIST` | 文件已存在 | `O_CREAT\|O_EXCL` 或 `mkfifo` |
| 22 | `EINVAL` | 参数无效 | `lseek` whence 错了？ |
| 28 | `ENOSPC` | 磁盘空间不足 | `df -h` |
| 32 | `EPIPE` | 管道破裂 | 读端关闭，写端需处理 SIGPIPE |
| 29 | `ESPIPE` | 不可 seek | 对管道/FIFO 用了 `lseek` |
| 107 | `ENOTDIR` | 路径中存在非目录 | 路径写错了 |

---

## 八、常见调试场景速查

### 场景 1：fork 后子进程行为异常

```bash
# 用 strace 分开看父和子
strace -f -o trace.log ./build/src/app/simulator

# 或只追踪子进程
strace -f -e follow-fork=child ./build/src/app/simulator
```

### 场景 2：程序启动后"卡住"

```bash
# 先 strace 看卡在哪个 syscall
strace -p <PID>

# 输出 "restart_syscall(<...>)" 或阻塞在 open/read — 说明在等 IO
```

### 场景 3：程序莫名其妙退出

```bash
# strace 看退出信号
strace -f ./build/src/app/simulator
# 看到 "+++ killed by SIGPIPE +++" → 管道破裂
# 看到 "+++ killed by SIGSEGV +++" → 段错误
```

### 场景 4：FIFO 数据没写进去

```bash
# 检查 FIFO 是否存在
test -p /tmp/temp_fifo && echo "exists" || echo "not found"

# 检查写入权限
ls -la /tmp/temp_fifo

# strace 看 write 返回值
strace -e trace=write ./build/src/app/simulator
# 看 write(4, "58.5\n", 5) = 5  表示写了 5 字节
```

### 场景 5：内存泄漏怀疑

```bash
# 编译时加 -g 保留符号
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# valgrind
timeout 5 valgrind --leak-check=full ./build/src/app/simulator
```

---

## 参考

- `man 1 strace`, `man 1 gdb`, `man 1 valgrind`
- `man 2 errno`, `man 3 errno`, `man 3 perror`, `man 3 strerror`
- `/usr/include/asm-generic/errno-base.h`（错误码 1-34）
- `/usr/include/asm-generic/errno.h`（错误码 35-133）
