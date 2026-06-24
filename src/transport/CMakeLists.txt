add_library(egw_transport STATIC
    egw_transport_serial.c
    egw_transport_tcp.c
    egw_transport_common.c
)

target_include_directories(egw_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(egw_transport PUBLIC egw_core)

install(TARGETS egw_transport
    LIBRARY   DESTINATION lib
    ARCHIVE   DESTINATION lib
    FILE_SET HEADERS DESTINATION include
)
