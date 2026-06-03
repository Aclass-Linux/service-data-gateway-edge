# app 学习记录

## 目录结构

```
src/app/
├── CMakeLists.txt    # 构建：gateway + simulator
├── main.c            # 入口（hello world 测试桩）
├── simulator.c       # M1 第1周：模拟数据源主程序
├── data_gen.c/h      # 温度生成模块
├── pipe_util.c/h     # 匿名管道工具
└── fifo_writer.c/h   # FIFO 写入模块
```

## 系统调用记录

| 调用 | 用途 | 首次使用文件 |
|---|---|---|
| `mkfifo()` | 创建命名管道 | `fifo_writer.c` |
| `pipe()` | 创建匿名管道 | `pipe_util.c` |
| `fork()` | 创建子进程 | `simulator.c` |
| `sigaction()` | 信号处理（SIGPIPE） | `fifo_writer.c` |
| `rand()` / `srand()` | 随机数生成 | `data_gen.c` |
| `kill()` | 向子进程发信号 | `simulator.c` |
| `wait()` | 等待子进程退出 | `simulator.c` |

## C11 特性使用记录

| 特性 | 文件 | 说明 |
|---|---|---|
| — | — | 暂未使用 |

## 常见问题

### 问题：FIFO 打开阻塞
- **现象**：以只写方式 `open()` FIFO 时进程卡住
- **原因**：FIFO 默认阻塞打开，直到另一端也有进程以只读方式打开
- **解决**：当前版本接受阻塞行为；第 2 周实现读端后自动解决

### 问题：C11 严格模式下 POSIX 函数不可见
- **现象**：`kill()` 声明为隐式，SIGTERM 未定义
- **原因**：`-std=c11` 定义 `__STRICT_ANSI__` 隐藏了 POSIX 扩展函数
- **解决**：CMakeLists.txt 中添加 `target_compile_definitions(simulator PRIVATE _POSIX_C_SOURCE=199309L)`

## 验证结果

- [x] 4.1 编译通过
- [x] 4.2 FIFO 数据流正常（58.5, 80.3, 33.5, 75.4）
- [x] 4.3 SIGPIPE 处理正常（进程不崩溃）

## 待办

- [ ] 实现 gateway_v1（第 2 周）
