# Transport 模块 Bug 清单

## 优先级

P0 = 必修（内存泄漏/逻辑错误）  
P1 = 建议修（行为不符合预期）  
P2 = 可以修（文档错误/一致性）

---

## Bug 1：读错误路径内存泄漏

- **文件**：`src/transport/egw_serial.c`
- **行号**：62–71
- **优先级**：P0

`egw_serial_on_read` 中 `nread < 0` 的错误路径：

```c
if (nread < 0) {
    if (tp->cbs.on_close) {
        egw_err_t err = (nread == UV_EOF) ? EGW_OK : EGW_ERR_READ;
        tp->cbs.on_close(tp, err);
    }
    free(buf->base);
    uv_read_stop(stream);
    uv_close((uv_handle_t *)stream, NULL);  // ← NULL 回调
    serial->opened = false;
    return;
}
```

`uv_close` 传了 `NULL` 回调，导致 `egw_serial_on_close_handle` 不被执行，以下资源从未释放：

- `serial` 结构体
- `serial->path_copy`（在 `egw_serial_on_close_handle` 中 `free`）
- `fd`（在 `egw_serial_on_close_handle` 中 `close`）

**修复方向**：将 `NULL` 改为 `egw_serial_on_close_handle`。

---

## Bug 2：读错误路径提前触发 on_close 回调

- **文件**：`src/transport/egw_serial.c`
- **行号**：63–65
- **优先级**：P1

同上路径，`tp->cbs.on_close` 在 `uv_close` 之前被调用。调用方收到回调时：

1. `uv_read_stop` 尚未执行
2. `uv_close` 尚未执行
3. `serial->opened` 尚未置 false

回调语义上，"close 完成"的通知应该等真正的清理完成后再触发。调用方可能在回调中尝试 reopen，但 transport 的实际状态尚未完成关闭。

**修复方向**：不在读错误路径中主动调 `on_close`，改为先发起 `uv_close`，让 `egw_serial_on_close_handle` 统一触发 `on_close`。

---

## Bug 3：注释说 uv_tty，实现用 uv_pipe

- **文件**：`src/transport/include/egw_serial.h` 第 5 行
- **优先级**：P2

```c
// 文件注释：
// 基于 libuv uv_tty 实现串口的异步打开/关闭/读写。
//            ^^^^^^ 错误，实际是 uv_pipe
```

实际 `egw_serial.c` 使用 `uv_pipe_init` + `uv_pipe_t`。

`egw_serial.c` 第 6 行的注释是正确的（"基于 libuv uv_pipe 实现"）。

**修复方向**：将 `uv_tty` 改为 `uv_pipe`。

---

## 受影响代码

| 文件 | 影响 |
|------|------|
| `src/transport/egw_serial.c` | Bug 1、Bug 2 |
| `src/transport/include/egw_serial.h` | Bug 3 |
