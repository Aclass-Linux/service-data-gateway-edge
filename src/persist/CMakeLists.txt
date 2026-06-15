add_library(egw_persist STATIC
    egw_persist.c
)

target_include_directories(egw_persist PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(egw_persist PUBLIC egw_core)

install(TARGETS egw_persist
    LIBRARY   DESTINATION lib
    ARCHIVE   DESTINATION lib
)
