#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/clean.sh — 清理构建产物

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
BUILD_DIR="${PROJECT_ROOT}/build"

if [ -d "$BUILD_DIR" ]; then
    echo "Removing $BUILD_DIR ..."
    rm -rf "$BUILD_DIR"
fi
echo "Done."