cmake_minimum_required(VERSION 3.20)

project(DataGatewayEdge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DGH_BUILD_TESTS "Build unit tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
include(GNUInstallDirs)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Install path prefix." FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/static")



add_subdirectory(src/core)
add_subdirectory(src/protocol)
add_subdirectory(src/data)
add_subdirectory(src/connectors)
add_subdirectory(src/hub)
add_subdirectory(src/app)

if(DGH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/core)
  add_subdirectory(tests/protocol)
  add_subdirectory(tests/data)
  add_subdirectory(tests/connectors)
  add_subdirectory(tests/hub)
endif()
