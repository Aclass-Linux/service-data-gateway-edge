# Core API

## 说明

本文档列出当前项目中实际存在的 API。不存在的 API 标注为"未实现"。

---

## 已实现

### `main` — 程序入口

```c
int main(void);
```

- **文件**：`src/app/main.c`
- **说明**：程序入口，当前输出 `"hello gateway"` 后返回 0
- **状态**：已实现

---

## 未实现

以下 API 在遗留文档中被引用，但项目当前版本中不存在。作为远期规划保留。

### Logger — 日志入口

```cpp
DataGatewayHub::Core::Logger
```

- **说明**：日志记录封装
- **状态**：未实现（M2+ 规划）

### Config — 配置存储

```cpp
DataGatewayHub::Core::Config
```

- **说明**：字符串键值对配置
- **状态**：未实现（M2+ 规划）

### ErrorCode — 错误码定义

```cpp
DataGatewayHub::Core::ErrorCode
```

- **说明**：共享状态值定义
- **状态**：未实现（M2+ 规划）
