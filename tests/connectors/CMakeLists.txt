add_executable(test_http_connector
  src/test_http_connector.cpp
)

target_link_libraries(test_http_connector
  PRIVATE
    dgh_connectors
)

add_test(NAME connectors_http COMMAND test_http_connector)

