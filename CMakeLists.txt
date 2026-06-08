cmake_minimum_required(VERSION 3.23)

# ── 工具链（必须在 project 之前）────────────────────
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/toolchain-${ARCH}.cmake)

# ── 项目名称 ────────────────────────────────────
set(ACLASS_PROJECT_NAME "AClassDemo_${ARCH}" CACHE STRING "Project name for output naming")
project(DataGatewayHub VERSION 0.1.0 LANGUAGES C)

# ── A类框架 ────────────────────────────────────
include(cmake/AClass.cmake)

add_subdirectory(third-party)
add_subdirectory(src)
