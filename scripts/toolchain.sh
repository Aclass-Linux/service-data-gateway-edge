#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/toolchain.sh — 架构检测、构建目录管理、submodule 检查

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
BUILD_DIR="${PROJECT_ROOT}/build"

_dgh_check_compiler() {
    if [ "$ARCH" = "armv7" ] && [ -n "${CROSS_COMPILE_PATH:-}" ]; then
        if [ ! -f "${CROSS_COMPILE_PATH}/arm-linux-gnueabihf-gcc" ]; then
            echo "Error: Cross compiler not found at ${CROSS_COMPILE_PATH}/arm-linux-gnueabihf-gcc"
            echo "Check CROSS_COMPILE_PATH in .project.local.config"
            return 1
        fi
    fi
}

_dgh_check_arch_change() {
    local arch_marker="${BUILD_DIR}/.build_arch"

    if [ -f "$arch_marker" ]; then
        local prev_arch
        prev_arch="$(cat "$arch_marker")"
        if [ "$prev_arch" != "$ARCH" ]; then
            echo "Architecture changed: $prev_arch -> $ARCH, cleaning build directory ..."
            rm -rf "$BUILD_DIR"
        fi
    fi
}

_dgh_check_submodules() {
    if [ -d "${PROJECT_ROOT}/third-party" ]; then
        local found_missing=false
        for submod_dir in "${PROJECT_ROOT}/third-party"/*/; do
            if [ -d "$submod_dir" ] && [ ! -f "${submod_dir}CMakeLists.txt" ] && [ ! -f "${submod_dir}Makefile" ]; then
                found_missing=true
                break
            fi
        done
        if $found_missing; then
            echo "Initializing submodules ..."
            command git -C "$PROJECT_ROOT" submodule update --init
        fi
    fi
}

_dgh_write_arch_marker() {
    mkdir -p "$BUILD_DIR"
    echo "$ARCH" > "${BUILD_DIR}/.build_arch"
}

_dgh_toolchain_file() {
    echo "${PROJECT_ROOT}/cmake/toolchain-${ARCH}.cmake"
}