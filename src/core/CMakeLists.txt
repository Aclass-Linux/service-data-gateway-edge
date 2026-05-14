add_library(dgh_core
  Logger.cpp
  Config.cpp
  ErrorCodes.cpp
)

target_include_directories(dgh_core
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(dgh_core PUBLIC cxx_std_17)
