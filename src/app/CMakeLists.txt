add_executable(data-gateway-edge
  main.cpp
)

target_link_libraries(data-gateway-edge
  PRIVATE

)

target_compile_features(data-gateway-edge PRIVATE cxx_std_17)
