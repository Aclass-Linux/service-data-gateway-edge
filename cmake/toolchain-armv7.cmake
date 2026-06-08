# cmake/toolchain-armv7.cmake
# armv7 交叉编译 — 从 .project.local.config 读取 COMPILE_PATH / SYSROOT_PATH

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(DEFINED COMPILE_PATH)
    set(CMAKE_C_COMPILER "${COMPILE_PATH}/arm-linux-gnueabihf-gcc")
endif()

if(DEFINED COMPILE_PATH)
    set(CMAKE_SYSROOT "${SYSROOT_PATH}")
endif()

# ── 编译器 & 汇编器 ────────────────────────────────
set(CMAKE_ASM_COMPILER   ${CMAKE_C_COMPILER})
set(CMAKE_LINKER         ${CMAKE_C_COMPILER})

if(DEFINED COMPILE_PATH)
    set(CMAKE_OBJCOPY    "${COMPILE_PATH}/arm-linux-gnueabihf-objcopy")
    set(CMAKE_SIZE       "${COMPILE_PATH}/arm-linux-gnueabihf-size")
else()
    set(CMAKE_OBJCOPY    arm-linux-gnueabihf-objcopy)
    set(CMAKE_SIZE       arm-linux-gnueabihf-size)
endif()

# ── 编译器默认行为（显式化，避免依赖编译器自身默认值）─
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_ASM_FLAGS       "-c")

# ── sysroot 查找策略 ──────────────────────────────
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)