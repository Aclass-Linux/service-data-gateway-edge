#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/release.sh — _egw_release 函数定义（支持独立运行）

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"

_egw_release() {
    export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:${PATH}"
    local BUILD_DIR="${PROJECT_ROOT}/build"
    local INSTALL_DIR="${PROJECT_ROOT}/install"

    rm -rf "$INSTALL_DIR"
    cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"
}

if [ -z "${_EGW_LOADED:-}" ]; then
    _egw_release
fi