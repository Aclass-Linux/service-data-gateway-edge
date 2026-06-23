add_library(egw_protocol STATIC
    egw_protocol.c
    egw_modbus.c
)

target_include_directories(egw_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(egw_protocol PUBLIC egw_core PRIVATE egw_transport)

install(TARGETS egw_protocol
    LIBRARY   DESTINATION lib
    ARCHIVE   DESTINATION lib
)
