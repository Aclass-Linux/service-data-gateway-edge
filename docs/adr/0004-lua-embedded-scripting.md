# ADR 0004: 使用 Lua 作为嵌入式脚本引擎

## 状态

已接受

## 背景

网关需要支持运维人员编写设备控制逻辑（如多步 Modbus 操作的事务性命令序列）。硬编码所有逻辑到 C 中不可行——每个现场的控制需求不同，必须通过脚本灵活编排。

脚本引擎需要：MIT 协议、轻量（ARMv7 128MB RAM 可用）、与 C 互操作良好、支持协程（配合 libuv 异步事件循环实现同步编码风格）。

## 决策

使用 Lua 5.4 作为嵌入式脚本引擎。

API 设计为同步协程风格：运维写 `local v = modbus:read(1, 30001)`，底层 C binding 向 event loop 发出 Modbus 请求后 `lua_yield()`，响应到达后 `lua_resume()` 将结果返回给脚本。脚本作者无需处理回调或 Promise。

重度计算脚本可以在未来调度到独立任务线程/线程池中执行，每个任务线程持有独立 `lua_State`，避免阻塞当前事件循环。第一版只实现单线程；线程模型的细节（每线程事件驱动 runtime、线程内单例、跨线程通过 uv 投递）见 ADR 0007。

## 否决的方案

**JavaScript (Duktape/QuickJS)**：语法受众广，但 ES 标准的异步模型（Promise/await）与 C 协程模型的映射不如 Lua 的 `yield/resume` 干净。Duktape 内存占用比 Lua 大。

**Python (MicroPython)**：生态丰富，但嵌入 C 的 API 复杂，运行时体积大（~500KB+），启动慢，不适合 ARMv7 资源受限场景。

**WASM (wasm3)**：支持多语言编译到 WASM，但需要完整的编译工具链和 AOT/JIT 依赖，增加运维复杂度。且 WASM 沙箱隔离的 I/O 桥接层需要额外设计。

**C 硬编码 + 参数配置**：删减了灵活性，每个控制逻辑都需要重新编译部署网关，无法适应交付后现场调整的需求。

## 理由

- **MIT 协议**：与项目整体协议一致，无 GPL 传染风险
- **极小体积**：<200KB，ARMv7 友好
- **协程原生支持**：`lua_yield` / `lua_resume` 完美匹配 libuv 异步模型，实现同步写法异步执行
- **C API 成熟**：Lua C API 稳定、文档齐全，nanomodbus + libuv 的跨语言桥接路径清晰
- **工业验证**：Redis、nginx、Wireshark 等广泛使用 Lua 嵌入，可靠性有保障
