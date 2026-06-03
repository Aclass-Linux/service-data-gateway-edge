## 1. 基础模块

- [x] 1.1 创建 `data_gen.h` 和 `data_gen.c`：实现随机温度值生成（20-100），使用 `rand()` + 信号安全初始化
- [x] 1.2 创建 `pipe_util.h` 和 `pipe_util.c`：封装 `pipe()` 创建、写端写入、读端读取

## 2. FIFO 模块

- [x] 2.1 创建 `fifo_writer.h` 和 `fifo_writer.c`：实现 `mkfifo()` 创建、`open()` 打开、写入、关闭
- [x] 2.2 处理 SIGPIPE 信号：`sigaction(SIGPIPE, ...)`，打印警告后继续运行

## 3. 主程序

- [x] 3.1 创建 `simulator.c`：fork 子进程，子进程运行 data_gen → 匿名管道，父进程运行 fifo_writer ← 匿名管道
- [x] 3.2 更新 `src/app/CMakeLists.txt`：添加 `simulator` 可执行目标，链接所有模块

## 4. 验证

- [x] 4.1 编译通过：`build` 后确认 simulator 可执行文件生成
- [x] 4.2 运行验证：启动 simulator，另一个终端 `cat /tmp/temp_fifo` 看到持续输出的温度数据
- [x] 4.3 信号处理验证：启动 simulator，不打开读端，进程不崩溃
