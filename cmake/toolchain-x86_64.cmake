# cmake/toolchain-x86_64.cmake
# x86_64 本地编译 — 从 .project.local.config 读取 COMPILE_PATH（可选）
# 未设置 COMPILE_PATH 时使用系统默认编译器

include(${CMAKE_CURRENT_LIST_DIR}/toolchain-linux-common.cmake)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(DEFINED COMPILE_PATH)
    set(CMAKE_C_COMPILER "${COMPILE_PATH}/gcc")
endif()

if(DEFINED COMPILE_PATH)
    set(CMAKE_SYSROOT "${SYSROOT_PATH}")
endif()

# ── 编译器 & 汇编器 ────────────────────────────────
set(CMAKE_ASM_COMPILER   ${CMAKE_C_COMPILER})
set(CMAKE_LINKER         ${CMAKE_C_COMPILER})
set(CMAKE_OBJCOPY        objcopy)
set(CMAKE_SIZE           size)

# ── 编译器默认行为（显式化，避免依赖编译器自身默认值）─
set(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_ASM_FLAGS       "-c")