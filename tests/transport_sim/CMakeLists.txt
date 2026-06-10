if(NOT ACLASS_ENABLE_TESTS)
    return()
endif()

# ── Unity ─────────────────────────────────────────────
find_package(Threads REQUIRED)

# ── 通用测试链接 ──────────────────────────────────────
set(EGW_TEST_LIBS unity egw_transport egw_core uv Threads::Threads)

# ── test_transport ─────────────────────────────────────
set(TEST_TRANSPORT_SRC test_transport.c)

add_executable(test_transport ${TEST_TRANSPORT_SRC})
target_link_libraries(test_transport PRIVATE ${EGW_TEST_LIBS})
target_include_directories(test_transport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/transport/include
    ${CMAKE_SOURCE_DIR}/src/transport
)
add_test(NAME test_transport COMMAND test_transport)

# ── test_serial ───────────────────────────────────────
set(TEST_SERIAL_SRC test_serial.c)

add_executable(test_serial ${TEST_SERIAL_SRC})
target_link_libraries(test_serial PRIVATE ${EGW_TEST_LIBS})
target_compile_definitions(test_serial PRIVATE _GNU_SOURCE)
target_include_directories(test_serial PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/transport/include
    ${CMAKE_SOURCE_DIR}/src/transport
)
add_test(NAME test_serial COMMAND test_serial)