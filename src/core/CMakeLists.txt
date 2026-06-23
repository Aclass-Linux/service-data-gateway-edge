set(core_src
    egw_err.c
    egw_bus.c
    egw_fsm.c
    egw_crc.c
)
if(USE_JSON_CONFIG)
    list(APPEND core_src config.c)
endif()

set(core_inc
    include/egw_defs.h
    include/egw_err.inc
    include/egw_ctype.inc
    include/config.h
    include/egw_fsm.h
    include/egw_bus.h
    include/egw_crc.h
)

add_library(egw_core STATIC)

target_sources(egw_core PRIVATE
    ${core_src}
)

target_sources(egw_core PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES     ${core_inc}
)

if(USE_JSON_CONFIG)
    target_compile_definitions(egw_core PUBLIC USE_JSON_CONFIG)
    target_link_libraries(egw_core PRIVATE cjson)
endif()

target_link_libraries(egw_core PUBLIC uv)

install(TARGETS egw_core
    LIBRARY   DESTINATION lib
    ARCHIVE   DESTINATION lib
    FILE_SET HEADERS DESTINATION include
)