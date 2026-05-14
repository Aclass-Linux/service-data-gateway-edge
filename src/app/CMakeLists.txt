add_executable(data-gateway-edge
  main.cpp
)

target_link_libraries(data-gateway-edge
  PRIVATE
    dgh_hub
)

target_compile_features(data-gateway-edge PRIVATE cxx_std_17)
