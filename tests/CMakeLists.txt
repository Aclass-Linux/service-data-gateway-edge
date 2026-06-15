add_subdirectory(transport_sim)

# ── test_egw_loop ──────────────────────────────────
add_executable(test_egw_loop test_egw_loop.c)
target_link_libraries(test_egw_loop PRIVATE uv)
add_test(NAME test_egw_loop COMMAND test_egw_loop)