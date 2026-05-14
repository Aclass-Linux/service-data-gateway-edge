add_executable(data-gateway-hub
  main.cpp
)

target_link_libraries(data-gateway-hub
  PRIVATE
    dgh_hub
)

target_compile_features(data-gateway-hub PRIVATE cxx_std_17)
