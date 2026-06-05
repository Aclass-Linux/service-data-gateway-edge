cmake_minimum_required(VERSION 3.23)

# ── 工具链（必须在 project 之前）────────────────────
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/toolchain-${ARCH}.cmake)

project(DataGatewayHub VERSION 0.1.0 LANGUAGES C)

include(cmake/AClass.cmake)

add_subdirectory(third-party/cjson)
add_subdirectory(src/core)
add_subdirectory(src/app)
