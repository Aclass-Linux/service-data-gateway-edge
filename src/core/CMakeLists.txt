add_library(dgh_core
  src/Logger.cpp
  src/Config.cpp
  src/ErrorCodes.cpp
)

target_include_directories(dgh_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(dgh_core PUBLIC cxx_std_17)

