# 实施步骤 1：Core 层事件循环封装 (egw_loop_t)

## 状态
实现中（待验证）

## 实施日期
2025-01-XX

## 目标
实现 Core 层对 libuv 事件循环的封装，成为事件循环的唯一所有者。

## 依赖
- libuv (third-party/libuv)

## 产出

### API 设计 (src/core/include/egw_loop.h)
```c
typedef struct egw_loop egw_loop_t;  /* 不透明句柄 */

egw_loop_t *egw_loop_create(void);
egw_err_t   egw_loop_run(egw_loop_t *loop);
void        egw_loop_stop(egw_loop_t *loop);
void        egw_loop_destroy(egw_loop_t *loop);

/* 内部访问接口（仅供Core层内部使用） */
void *egw_loop_get_uv_loop(egw_loop_t *loop);
```

### 内部结构 (src/core/egw_loop.c)
```c
struct egw_loop {
    uv_loop_t uv_loop;
    bool      should_stop;
};
```

### 生命周期语义
1. **创建**：`egw_loop_create()` 分配内存并初始化 `uv_loop_t`
2. **运行**：`egw_loop_run()` 阻塞运行事件循环，直到无活动句柄或显式停止
3. **停止**：`egw_loop_stop()` 可从信号处理器或回调中调用，标记停止并调用 `uv_stop()`
4. **销毁**：`egw_loop_destroy()` 关闭所有未关闭的句柄，清理资源

### 测试 (tests/test_egw_loop.c)
- 创建事件循环
- 注册 SIGINT 信号处理器验证 `egw_loop_stop()`
- 创建定时器验证事件循环正常工作（每秒触发，3次后自动停止）
- 销毁事件循环

## 构建集成

### CMake 修改
1. `src/core/CMakeLists.txt`：
   - 添加 `egw_loop.c` 到源文件列表
   - 添加 `egw_loop.h` 到公开头文件
   - 链接 `uv_a` 库
   - 添加 libuv include 路径：`${PROJECT_SOURCE_DIR}/third-party/libuv/include`

2. `tests/CMakeLists.txt`：
   - 添加 `test_egw_loop` 可执行文件
   - 链接 `egw_core` 库

3. `src/core/include/egw_err.inc`：
   - 新增错误码 `ERR_LOOP_RUN (-20)`
   - 移除重复的 `ERR_INVALID_ARG` 别名（与 `ERR_INVAL` 冲突）

## 验证计划
1. 编译通过（无警告）
2. 运行 `test_egw_loop`：
   - 定时器每秒触发，输出 "Timer tick 1/2/3"
   - 3 次后自动停止，输出 "Stopping loop after 3 ticks"
   - 程序正常退出，输出 "All tests passed!"
3. 手动测试 SIGINT：运行测试程序后按 Ctrl+C，验证优雅退出

## 遗留问题
- 构建系统配置完成，待 classifier 恢复后执行构建验证

## 下一步
验证通过后，更新到主文档，开始第 2 步：实现 `egw_runtime_t`（运行时单例）。
