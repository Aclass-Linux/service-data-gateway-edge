add_library(egw_ptable STATIC
    egw_ptable_loader.c
)

target_include_directories(egw_ptable PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(egw_ptable PUBLIC egw_core)

install(TARGETS egw_ptable
    LIBRARY   DESTINATION lib
    ARCHIVE   DESTINATION lib
)
