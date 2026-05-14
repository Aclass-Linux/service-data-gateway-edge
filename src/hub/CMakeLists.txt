add_library(dgh_hub
  src/HubEngine.cpp
  src/DdlExecutor.cpp
)

target_include_directories(dgh_hub
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dgh_hub
  PUBLIC
    dgh_core
    dgh_protocol
    dgh_data
    dgh_connectors
)

target_compile_features(dgh_hub PUBLIC cxx_std_17)

