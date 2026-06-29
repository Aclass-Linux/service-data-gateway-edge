add_library(egw_ptable STATIC
    egw_ptable.c
    egw_persist.c
)

target_include_directories(egw_ptable PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(egw_ptable PUBLIC egw_core sqlite3)

install(TARGETS egw_ptable
    LIBRARY   DESTINATION lib
    ARCHIVE   DESTINATION lib
)
