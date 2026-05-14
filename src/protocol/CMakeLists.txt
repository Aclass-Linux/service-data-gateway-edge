add_library(dgh_protocol
  Packet.cpp
  FrameBuilder.cpp
)

target_include_directories(dgh_protocol
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(dgh_protocol
  PUBLIC
    dgh_core
)

target_compile_features(dgh_protocol PUBLIC cxx_std_17)
