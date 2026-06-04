#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/build.sh — 配置并编译（支持独立运行）

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
CONFIG_FILE="${PROJECT_ROOT}/.project.config"
LOCAL_CONFIG_FILE="${PROJECT_ROOT}/.project.local.config"
BUILD_DIR="${PROJECT_ROOT}/build"

if [ -z "${ARCH:-}" ]; then
    ARCH=x86_64
    DGH_LINK=shared
    CMAKE_BUILD_TYPE=Debug
    if [ -f "$CONFIG_FILE" ]; then
        # shellcheck source=/dev/null
        . "$CONFIG_FILE"
    fi
    if [ -f "$LOCAL_CONFIG_FILE" ]; then
        # shellcheck source=/dev/null
        . "$LOCAL_CONFIG_FILE"
    fi
    export ARCH DGH_LINK CMAKE_BUILD_TYPE
fi

# shellcheck source=/dev/null
. "${PROJECT_ROOT}/scripts/toolchain.sh"

if ! _dgh_check_compiler; then
    exit 1
fi

_dgh_check_submodules
_dgh_check_arch_change

if ! command -v ninja >/dev/null 2>&1; then
    echo "Error: ninja is required but not found."
    echo "Install it with: sudo apt install ninja-build"
    exit 1
fi

TOOLCHAIN_FILE="$(_dgh_toolchain_file)"

cmake_args=(
    -G Ninja
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
    -DDGH_LINK="$DGH_LINK"
)

if [ "$ARCH" = "armv7" ]; then
    cmake_args+=(-DCMAKE_INSTALL_PREFIX="${PROJECT_ROOT}/install")
fi

cmake "${cmake_args[@]}" "$PROJECT_ROOT"
cmake --build "$BUILD_DIR" "$@"

_dgh_write_arch_marker