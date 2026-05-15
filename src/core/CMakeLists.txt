add_library(core
  Aclass_core.h

)

target_include_directories(core
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(core PUBLIC cxx_std_17)
