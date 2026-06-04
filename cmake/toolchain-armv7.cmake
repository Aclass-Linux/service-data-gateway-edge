# cmake/toolchain-armv7.cmake
# armv7 交叉编译 — 引用环境变量 CROSS_COMPILE_PATH / SYSROOT_PATH
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT DEFINED ENV{CROSS_COMPILE_PATH})
    message(FATAL_ERROR "CROSS_COMPILE_PATH is not set. Set it in .project.local.config")
endif()

if(NOT DEFINED ENV{SYSROOT_PATH})
    message(FATAL_ERROR "SYSROOT_PATH is not set. Set it in .project.local.config")
endif()

set(CMAKE_C_COMPILER "$ENV{CROSS_COMPILE_PATH}/arm-linux-gnueabihf-gcc")
set(CMAKE_SYSROOT "$ENV{SYSROOT_PATH}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)