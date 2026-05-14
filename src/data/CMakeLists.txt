add_library(dgh_data
  Repository.cpp
  ConnectionPool.cpp
)

target_include_directories(dgh_data
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(dgh_data
  PUBLIC
    dgh_core
)

target_compile_features(dgh_data PUBLIC cxx_std_17)
