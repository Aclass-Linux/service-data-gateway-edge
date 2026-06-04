#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/build.sh — _dgh_build 函数定义（支持独立运行）

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"

_dgh_build() {
    export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:${PATH}"
    local BUILD_DIR="${PROJECT_ROOT}/build"

    # shellcheck source=/dev/null
    . "${PROJECT_ROOT}/scripts/toolchain.sh"

    if ! _dgh_check_compiler; then
        return 1
    fi

    _dgh_check_submodules
    _dgh_check_arch_change

    if ! command -v ninja >/dev/null 2>&1; then
        echo "Error: ninja is required but not found."
        echo "Install it with: sudo apt install ninja-build"
        return 1
    fi

    local TOOLCHAIN_FILE
    TOOLCHAIN_FILE="$(_dgh_toolchain_file)"

    local cmake_args=(
        -G Ninja
        -B "$BUILD_DIR"
        -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
        -DDGH_LINK="$DGH_LINK"
    )

    if [ "$ARCH" = "armv7" ]; then
        cmake_args+=(-DCMAKE_INSTALL_PREFIX="${PROJECT_ROOT}/install")
    fi

    cmake "${cmake_args[@]}" "$PROJECT_ROOT" || {
        if $_dgh_submodules_missing; then
            echo "Tip: Try 'submodule-sync' to sync submodules"
        fi
        return 1
    }
    cmake --build "$BUILD_DIR" "$@" || return 1

    _dgh_write_arch_marker
}

# 独立运行时自加载配置并执行
if [ -z "${_DGH_LOADED:-}" ]; then
    CONFIG_FILE="${PROJECT_ROOT}/.project.config"
    LOCAL_CONFIG_FILE="${PROJECT_ROOT}/.project.local.config"

    ARCH=x86_64; DGH_LINK=shared; CMAKE_BUILD_TYPE=Debug
    if [ -f "$CONFIG_FILE" ]; then . "$CONFIG_FILE"; fi
    if [ -f "$LOCAL_CONFIG_FILE" ]; then . "$LOCAL_CONFIG_FILE"; fi
    export ARCH DGH_LINK CMAKE_BUILD_TYPE

    _dgh_build "$@"; exit $?
fi