cmake_minimum_required(VERSION 3.20)

project(DataGatewayHub VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DGH_BUILD_TESTS "Build unit tests" ON)
option(DGH_BUILD_SAMPLES "Build sample programs" ON)

include(GNUInstallDirs)

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

if(DGH_BUILD_SAMPLES)
  add_subdirectory(samples/basic_usage)
endif()
