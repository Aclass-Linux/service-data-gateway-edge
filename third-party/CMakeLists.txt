# ── cJSON ──────────────────────────────────────────
add_subdirectory(cjson)

# ── libuv ─────────────────────────────────────────
add_subdirectory(libuv)

# ── Unity ─────────────────────────────────────────
if(ACLASS_ENABLE_TESTS)
    add_subdirectory(unity)
endif()