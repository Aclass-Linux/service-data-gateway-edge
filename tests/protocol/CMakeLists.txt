add_executable(test_packet
  src/test_packet.cpp
)

target_link_libraries(test_packet
  PRIVATE
    dgh_protocol
)

add_test(NAME protocol_packet COMMAND test_packet)

