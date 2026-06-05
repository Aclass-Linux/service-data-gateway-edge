#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/clean.sh — _egw_clean 函数定义（支持独立运行）

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"

_egw_clean() {
    local BUILD_DIR="${PROJECT_ROOT}/build"

    if [ -d "$BUILD_DIR" ]; then
        echo "Removing $BUILD_DIR ..."
        rm -rf "$BUILD_DIR"
    fi
    echo "Done."
}

if [ -z "${_EGW_LOADED:-}" ]; then
    _egw_clean
fi