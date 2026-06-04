#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/release.sh — 安装到 install/ 目录

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
BUILD_DIR="${PROJECT_ROOT}/build"
INSTALL_DIR="${PROJECT_ROOT}/install"

rm -rf "$INSTALL_DIR"
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"