# CMake 项目组织方案对比

## 背景

CMakeLists.txt 该怎么拆、拆多细，是 C++ 项目的一个经典设计问题。这里对比三种主流方案。

---

## 方案一：单文件集中式

```
CMakeLists.txt          # 全部内容
src/
  main.c
  core.c
  protocol.c
  data.c
```

### 做法

所有 target、编译选项、依赖关系全写在根目录一个文件里。

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(MyApp LANGUAGES C CXX)

add_executable(myapp
  src/main.c
  src/core.c
  src/protocol.c
)

target_include_directories(myapp PRIVATE include)
target_link_libraries(myapp PRIVATE some_external_lib)
```

### 优点

- 文件少，打开一个 CMakeLists.txt 就能看清所有 target
- 不需要在文件间跳转，变量作用域不跨文件，心智负担低
- 重构 target 时不用跨文件改

### 缺点

- 源文件一多（50+），文件顶部和底部隔几百行，改一次要滚半天
- 新人想看「某个模块链接了什么库」需要全文搜索
- 无法按模块独立开关编译（比如测试开关、工具开关）
- 一旦构建系统有了条件逻辑（if/else），文件会迅速膨胀

### 适用场景

- 小型项目（< 10 个源文件）
- 快速原型验证
- 个人项目，不预期会大幅增长

---

## 方案二：按模块拆分子目录（本工程采用）

```
CMakeLists.txt                     # 根 → 聚合选项、子目录、全局编译选项
src/
  core/
    CMakeLists.txt                  # header-only 库
    Aclass_core.h
  app/
    CMakeLists.txt                  # 主入口可执行文件
    main.c
  protocol/                         # 预留
    CMakeLists.txt
  data/
    CMakeLists.txt
  connectors/
    CMakeLists.txt
  hub/
    CMakeLists.txt
```

### 做法

根 CMakeLists.txt 只负责：
- 工程元信息（project version, language）
- 全局编译选项（C++ 标准、警告）
- 用 `add_subdirectory` 委托给子目录

每个子目录的 CMakeLists.txt 只负责：
- 声明本模块的 target
- 本模块的源文件
- 直接依赖的链接关系

```cmake
# src/app/CMakeLists.txt
add_executable(data-gateway-edge main.c)
target_link_libraries(data-gateway-edge PRIVATE core)
```

```cmake
# src/core/CMakeLists.txt
add_library(core INTERFACE)
target_include_directories(core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
```

### 优点

- **关注点分离**：根管「全局」，子目录管「局部」
- **增量构建快**：只改动一个模块的源文件，CMake 不需要重新解析整个树
- **新模块成本低**：新建目录 + 写 3 行 CMakeLists.txt + 根目录加一行 add_subdirectory
- **天然防循环依赖**：`target_link_libraries` 的依赖方向一目了然
- **并行开发友好**：每人负责一个子目录，git 冲突少

### 缺点

- 需要跨文件跳转（根 → subdirectory → subdirectory）
- 子目录 CMakeLists.txt 中有少量模板化内容（每加一个目录都要写 add_subdirectory）
- 过度拆分会导致文件碎片（比如一个只有 3 行的 cmake 文件单独成页）

### 适用场景

- 中等规模项目（10-100+ 源文件）
- 多人协作
- 有明显模块边界的项目
- **本工程当前阶段最推荐的方案**

---

## 方案三：独立 CMake 工程 + FetchContent

```
project-root/
  CMakeLists.txt                   # 主工程
  src/app/
    CMakeLists.txt
  deps/
    core/                          # 独立工程，有自己单独的 CMakeLists.txt
      CMakeLists.txt               # project(core ...)
    protocol/
      CMakeLists.txt               # project(protocol ...)
```

### 做法

每个模块都是一个独立的 CMake 工程（有自己的 `project()` 声明），通过 `add_subdirectory` 或 `FetchContent` 拉取。

```cmake
# 根 CMakeLists.txt
include(FetchContent)
FetchContent_Declare(core
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/core
)
FetchContent_MakeAvailable(core)
```

```cmake
# deps/core/CMakeLists.txt（独立的工程）
cmake_minimum_required(VERSION 3.20)
project(core VERSION 0.1.0 LANGUAGES C CXX)
add_library(core STATIC core.c)
```

### 优点

- **模块可以被其他项目独立复用**——不需要复制粘贴，用 FetchContent 直接拉
- **模块可以独立指定编译器版本、C++ 标准、编译选项**
- 模块内部修改不影响外部构建系统

### 缺点

- **过度设计的概率极高**——大部分模块永远不会被复用
- FetchContent 下载/配置会拖慢首次 cmake 速度
- 模块间交互复杂（需要显式 export/import target）
- 调试困难（跨工程符号跳转、断点需要 IDE 支持）

### 适用场景

- 跨项目复用的公共库（如公司内部的通信库、算法库）
- 大型组织中的微服务式 C++ 架构

---

## 对比总结

| 维度 | 单文件 | 按模块子目录（本工程） | 独立工程 + FetchContent |
|------|--------|----------------------|------------------------|
| 文件数 | 1 个 | 5~10 个 | 10+ 个 |
| 构建分层 | 无 | 根/子两层 | 多工程嵌套 |
| 模块复用 | 不可 | 不直接支持 | 天然支持 |
| 增量配置速度 | 全量重解析 | 只变改过的目录 | 拉取依赖慢 |
| 新人上手成本 | 低 | 中 | 高 |
| 增减模块成本 | 低（文件变大） | 低（加一行） | 中（声明依赖图） |
| 防循环依赖 | 靠自觉 | CMake 可检测 | 强制隔离 |
| 推荐项目规模 | 小型（<10 文件） | 中型（10~100+） | 大型组织 / 跨项目复用 |

## 本工程的选择逻辑

当前采用**方案二（按模块子目录）**，原因：

1. 项目有清晰的层级边界（core → protocol → data → connectors → hub → app）
2. 模块间的依赖方向是固定的（上层依赖下层，不允许下层依赖上层），子目录结构天然约束了这一点
3. 需要同时支持 x86 和 ARM32 交叉编译，模块化便于未来按需替换底层实现
4. 不需要跨项目复用（那是方案三的领域，以后真有需要再拆）

> 不要在第一天就为「以后可能复用」买单。等到第二个项目真的需要拉取 core 时，再把它拆成独立工程也不迟。

---

## 附：main() 为什么单独做一个 add_executable

### 四种写法的对比

给定一个简单的项目结构：

```
src/
  app/main.c          ← 入口 main()
  core/core.c          ← 核心逻辑
  core/core.h
```

#### 写法 A：全部塞进 executable

```cmake
add_executable(myapp
  main.c
  core.c
)
```

不需要库，最直接。

**优点**：文件最少、修改最直接。
**缺点**：core.c 里的函数无法被单元测试直接链接（executable 不能被 link）；如果未来有第二个可执行文件（比如命令行工具），core.c 会被编译两次。

---

#### 写法 B：thin executable + 库 ✅ 本工程采用

```cmake
# src/app/CMakeLists.txt
add_executable(data-gateway-edge main.c)
target_link_libraries(data-gateway-edge PRIVATE core)

# src/core/CMakeLists.txt
add_library(core STATIC core.c)
```

**优点**：
- core 可以被单元测试直接 `target_link_libraries`，不需要 mock 入口
- 第二个可执行文件只需要 link 同一个 core，不重复编译
- 编译时只重新编译改过的模块
- main.c 里只有「接线」代码

**缺点**：
- 多一个 CMakeLists.txt
- main 想调用 core 的函数时，需要显式 link

---

#### 写法 C：fat library + stub executable（Google Test 风格的 main 测试）

```cmake
# src/app/CMakeLists.txt
add_library(app_main STATIC main.c)         # main() 本身也做成库
add_executable(data-gateway-edge $<TARGET_OBJECTS:app_main>)
target_link_libraries(data-gateway-edge PRIVATE core)

# tests/CMakeLists.txt
add_executable(test_main main.c)             # 测试用另一个 main()
target_link_libraries(test_main PRIVATE app_main core)
```

这里故意没有用 `target_link_libraries(data-gateway-edge PRIVATE app_main)`，是因为即使把 main() 放进库，链接器有 `-Wl,-e main` 入口约束，直接 link 会冲突。正确做法是用 `$<TARGET_OBJECTS:...>` 对象库，或者把除 main() 以外的所有代码放进库。

更常见的变体：

```cmake
add_library(core STATIC core.c)        # 核心逻辑
add_library(app_main OBJECT main.c)    # main.c 作为对象库
add_executable(data-gateway-edge $<TARGET_OBJECTS:app_main>)
target_link_libraries(data-gateway-edge PRIVATE core)

add_executable(test_runner test_main.cpp)
target_link_libraries(test_runner PRIVATE core)
```

**优点**：连 main() 都可以被测试覆盖到（mock argv、测试不同初始化路径）。
**缺点**：过度设计，除非你有多个不同的 main 需要复用同一套初始化代码，否则不划算。

---

#### 写法 D：没有 executable，只有库

```cmake
add_library(data-gateway-edge STATIC
  main.c
  core.c
)
```

嵌入式 SDK、WebAssembly、Unity 插件等场景。

**优点**：调用方自己决定入口。
**缺点**：不能 `./myapp` 直接运行，需要一个宿主程序来调用。

---

### 本工程的选择原因

当前采用 **写法 B**，因为：

1. **测试是第一考量**：core 必须能被单元测试单独链接。如果测试代码 `#include` 了 core.h 但 core.c 没被编译进去，链接就会失败——这个约束迫使模块边界保持清晰。
2. **main() 应该是项目里最无聊的文件**：解析参数 → 初始化库 → 调用库的 run 函数 → 返回。如果 main.c 超过 30 行，说明有逻辑应该被抽到库里。
3. **什么时候升级到写法 C**：当你需要测试 `main()` 本身的不同行为（如命令行解析逻辑），或者有两个可执行文件共享同一套初始化流程时。
