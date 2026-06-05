# cmake/toolchain-x86_64.cmake
# x86_64 本地编译 — 从 .project.local.config 读取 COMPILE_PATH（可选）
# 未设置 COMPILE_PATH 时使用系统默认编译器

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(DEFINED COMPILE_PATH)
    set(CMAKE_C_COMPILER "${COMPILE_PATH}/gcc")
endif()

if(DEFINED COMPILE_PATH)
    set(CMAKE_SYSROOT "${SYSROOT_PATH}")
endif()