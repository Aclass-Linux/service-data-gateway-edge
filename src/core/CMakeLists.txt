add_library(core INTERFACE)
target_sources(core INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES Aclass_core.h
)

target_include_directories(core
  INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(core INTERFACE cxx_std_17)
