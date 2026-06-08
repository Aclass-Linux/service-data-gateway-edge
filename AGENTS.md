# AGENTS.md

## Build

All builds must start by sourcing the environment:

```bash
source aclass.env.sh
```

This provides: `build`, `release`, `clean`, `rebuild`, `run`, `submodule-add`, `submodule-rm`, `submodule-sync`

- `build` — cmake configure + compile (uses Ninja; `ninja` must be installed)
- `release` — sets `CMAKE_BUILD_TYPE=Release`, builds, installs to `install/`
- `run` — executes the gateway binary (x86_64 only; armv7 will refuse)
- Switching `ARCH` in `.project.config` auto-cleans `build/` on next build

No test, lint, or typecheck commands exist yet.

## Configuration

- `.project.config` — shared build params (committed). Every `KEY=VAL` line is auto-extracted and passed to CMake as `-DKEY=VAL`.
- `.project.local.config` — local overrides (gitignored). `COMPILE_PATH` / `SYSROOT_PATH` for cross-compile toolchains go here. Local file is sourced first, then overwritten by the shared config for same-key variables.
- `ARCH` controls toolchain selection: `x86_64` → `cmake/toolchain-x86_64.cmake`, `armv7` → `cmake/toolchain-armv7.cmake`.

## Project Structure

```
src/app/      → executable (main.c), target name = ACLASS_PROJECT_NAME (default: EdgeGateWay)
src/core/     → egw_core static library (config.c, defs, headers)
  include/    → public headers (config.h, egw_defs.h)
third-party/  → git submodules (cjson)
cmake/        → AClass.cmake (shared CMake settings), toolchain files
scripts/      → build.sh, release.sh, clean.sh, submodule.sh
```

## Conventions

- C11 strictly (`CMAKE_C_EXTENSIONS OFF`). GCC/Clang only (`-Wall -Wextra`; `-Werror` in Release).
- Module auto-init: `EGW_EXPORT(func, prio)` defines a function that runs before `main()` via `__attribute__((constructor))`. Priority offset is +101 internally to avoid compiler-reserved range 0–100.
- Error codes: `egw_err_t` (int32_t). `EGW_OK = 0`, negatives for errors (`EGW_ERR_FILE_NOT_FOUND`, `EGW_ERR_PARSE`, etc.). Append new codes at the end to preserve values.
- Config API uses dot-notation key paths with array indexing: `"modbus.serial_ports[0].path"`. Prefer convenience macros `EGW_CONF_STR()`, `EGW_CONF_INT()`, `EGW_CONF_BOOL()` over direct function calls.
- Binary outputs go to `build/bin/` (runtime) and `build/lib/` (libraries). Install target is `install/`.
- Add third-party deps as git submodules under `third-party/`; use `submodule-add <url> <path> [tag]` and register in `third-party/CMakeLists.txt`.