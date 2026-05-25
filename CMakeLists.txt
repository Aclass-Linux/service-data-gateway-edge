# ============================================================
# DataGatewayEdge — 边缘网关服务
# ============================================================

# ---- [1] 工程基本信息 ----
cmake_minimum_required(VERSION 3.20)
project(DataGatewayEdge
  VERSION     0.1.0
  DESCRIPTION "Aclass edge data gateway service"
  LANGUAGES   C CXX          # 同时启用 C 和 C++ 编译器
)

# ---- [2] C++ 标准（可通过 -DCMAKE_CXX_STANDARD=20 覆盖） ----
set(CMAKE_CXX_STANDARD 17           CACHE STRING "C++ language standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON  CACHE BOOL   "Reject compiler downgrade")
set(CMAKE_CXX_EXTENSIONS OFF        CACHE BOOL   "Disable compiler extensions")

# ---- [3] 构建选项 ----
#   所有选项均可在命令行通过 -D 覆盖，或在 .project-config 中配置
option(ACLASS_BUILD_TESTS     "Build unit tests"                     ON)
option(ACLASS_STRICT_WARNINGS "Enable extra compiler warnings"       ON)
option(BUILD_SHARED_LIBS      "Build shared libraries instead of static" OFF)

# ---- [4] 安装路径 ----
include(GNUInstallDirs)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH
    "Install path prefix." FORCE)
endif()

# ---- [5] 输出目录 ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/static")

# ============================================================
# 编译器选项
# ============================================================

# ---- [6] 严格编译警告 ----
if(ACLASS_STRICT_WARNINGS)
  # 同时检查 C 和 C++ 编译器，避免交叉编译时漏检
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang"
      OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")

    # -- 通用（C 和 C++） --
    add_compile_options(-Wall -Wextra -Wpedantic)
    # 开发阶段用 debug 模式，不阻断编译；release 才启用 -Werror
    add_compile_options($<$<CONFIG:Release>:-Werror>)
    add_compile_options(-Wshadow -Wformat=2 -Wundef)
    add_compile_options(-Wcast-align -Wpointer-arith)

    # -- C 专用 --
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wold-style-definition>)

    # -- C++ 专用 --
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>)

  elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX)
  endif()
endif()

# ============================================================
# 子目录
# ============================================================

add_subdirectory(src)

# ---- [7] 测试 ----
if(ACLASS_BUILD_TESTS)
  enable_testing()
  # add_subdirectory(tests)   # 后续加入测试目录
endif()

# ---- [8] 安装规则（TODO） ----
# install(TARGETS data-gateway-edge RUNTIME DESTINATION bin)
