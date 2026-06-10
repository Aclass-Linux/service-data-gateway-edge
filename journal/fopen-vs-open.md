# `fopen` vs `open` — 取舍决策笔记

## 一句话结论

**需要精细 I/O 控制 → `open`** ｜**只需要读/写文件内容 → `fopen`**

## 对比

| | `fopen` + `FILE *` | `open` + `int fd` |
|---|---|---|
| 归属 | ISO C 标准 | POSIX |
| 返回 | `FILE *` 流指针 | `int` 文件描述符 |
| 缓冲 | 用户态缓冲（默认 4K/8K） | 无缓冲，每次调用都是系统调用 |
| 可移植 | 任何 C 编译器 | 仅类 Unix |
| 适用场景 | 读配置、日志、文本处理 | 管道、socket、mmap、termios、fsync |
| 大文件 | `fseek` 的 `long` 有溢出风险 | `lseek` / `stat` 用 `off_t`（64 位安全） |

## 大型项目怎么选

| 项目 | 读配置 | 生产 I/O |
|------|--------|---------|
| **Nginx** | `open` + `pread`（流式 tokenizer，需精确控制 offset） | `open` + `sendfile` / `epoll` |
| **FFmpeg** | `fopen`（工具函数） | `open` + `pread` / `mmap`（性能路径） |
| **Redis** | `fopen` + `fgets`（逐行解析方便） | `open` + `write` + `fsync`（持久化） |
| **Git** | `fopen`（配置解析） | `open` + `mmap`（对象存储） |

**混合是常态。** 同一项目根据场景混用两种 API。

## 本项目实践

| 模块 | 选择 | 理由 |
|------|------|------|
| `config.c`（配置加载） | `fopen` + 动态增长 | 纯 C11，一次读完，无特殊控制需求 |
| `egw_serial.c`（串口） | `open` + `int fd` | 必须传给 libuv、termios，无法用 `FILE *` |

## 参考

- Nginx `ngx_conf_file.c`：`open` + `pread` 分块流式解析
- FFmpeg：`fopen` 在工具函数，`open` 在性能/控制路径
- 本笔记独立于 `journal/2026-06-03-fileio-pipes.md`（后者偏系统调用细节），专注 API 选择决策
