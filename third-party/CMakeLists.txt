# ── cJSON ──────────────────────────────────────────
add_subdirectory(cjson)

# ── libuv ─────────────────────────────────────────
add_subdirectory(libuv)

# ── SQLite ────────────────────────────────────────
add_subdirectory(sqlite)

# ── nanoMODBUS ─────────────────────────────────────
add_subdirectory(nanoModbus)

# ── Unity ─────────────────────────────────────────
if(ACLASS_ENABLE_TESTS)
    add_subdirectory(unity)
endif()