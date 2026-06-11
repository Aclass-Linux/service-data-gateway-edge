set(tp_src
    egw_transport.c
    egw_serial.c
)

set(tp_inc
    include/egw_transport.h
    include/egw_serial.h
    include/egw_serial_params.h
)

add_library(egw_transport STATIC)

target_sources(egw_transport PRIVATE
    ${tp_src}
)

target_sources(egw_transport PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES     ${tp_inc}
)

target_link_libraries(egw_transport PUBLIC uv egw_core)

install(TARGETS egw_transport
    LIBRARY   DESTINATION lib
    ARCHIVE   DESTINATION lib
    FILE_SET HEADERS DESTINATION include
)