add_library(egw_protocol STATIC
    egw_modbus_core.c
    egw_modbus_client.c
    egw_modbus_server.c
)

target_include_directories(egw_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(egw_protocol PUBLIC egw_core)

install(TARGETS egw_protocol
    LIBRARY   DESTINATION lib
    ARCHIVE   DESTINATION lib
)
