add_library(dgh_connectors
  HttpConnector.cpp
  MqttConnector.cpp
)

target_include_directories(dgh_connectors
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(dgh_connectors
  PUBLIC
    dgh_core
)

target_compile_features(dgh_connectors PUBLIC cxx_std_17)
