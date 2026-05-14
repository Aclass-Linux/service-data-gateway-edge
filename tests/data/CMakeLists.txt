add_executable(test_repository
  src/test_repository.cpp
)

target_link_libraries(test_repository
  PRIVATE
    dgh_data
)

add_test(NAME data_repository COMMAND test_repository)

