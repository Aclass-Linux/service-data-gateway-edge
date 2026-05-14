add_executable(sample_basic_usage
  main.cpp
)

target_link_libraries(sample_basic_usage
  PRIVATE
    dgh_hub
)

