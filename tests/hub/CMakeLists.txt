add_executable(test_hub_engine
  src/test_hub_engine.cpp
)

target_link_libraries(test_hub_engine
  PRIVATE
    dgh_hub
)

add_test(NAME hub_engine COMMAND test_hub_engine)

