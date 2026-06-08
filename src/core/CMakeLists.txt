if(NOT DEFINED USE_JSON_CONFIG)
    set(USE_JSON_CONFIG ON)
endif()



set(core_src 
    config.c
)

set(core_inc 
    include/egw_defs.h
    include/config.h
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

target_link_libraries(egw_core PRIVATE 
    cjson)

install(TARGETS egw_core
    LIBRARY   DESTINATION lib     # .so
    ARCHIVE   DESTINATION lib     # .a
    FILE_SET HEADERS DESTINATION include
)