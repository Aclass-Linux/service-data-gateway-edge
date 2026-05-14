add_executable(test_logger
  src/test_logger.cpp
)

target_link_libraries(test_logger
  PRIVATE
    dgh_core
)

add_test(NAME core_logger COMMAND test_logger)

