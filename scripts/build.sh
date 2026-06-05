#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/build.sh — _egw_build 函数定义（支持独立运行）

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"

_egw_build() {
    export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:${PATH}"
    local BUILD_DIR="${PROJECT_ROOT}/build"

    _egw_check_submodules

    # 架构变更检测：ARCH 变了就清空 build 目录
    local arch_marker="${BUILD_DIR}/.build_arch"
    if [ -f "$arch_marker" ]; then
        local prev_arch
        prev_arch="$(cat "$arch_marker")"
        if [ "$prev_arch" != "$ARCH" ]; then
            echo "Architecture changed: $prev_arch -> $ARCH, cleaning build directory ..."
            rm -rf "$BUILD_DIR"
        fi
    fi

    if ! command -v ninja >/dev/null 2>&1; then
        echo "Error: ninja is required but not found."
        echo "Install it with: sudo apt install ninja-build"
        return 1
    fi

    local cmake_args=(
        -G Ninja
        -B "$BUILD_DIR"
        -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    )

    if [ "$ARCH" = "armv7" ]; then
        cmake_args+=(-DCMAKE_INSTALL_PREFIX="${PROJECT_ROOT}/install")
    fi

    # 自动提取配置文件中的所有变量传给 cmake
    for f in "${PROJECT_ROOT}/.project.local.config" "${PROJECT_ROOT}/.project.config"; do
        [ -f "$f" ] || continue
        while IFS='=' read -r key val; do
            case "$key" in
                ''|'#'*) continue ;;
                *) cmake_args+=(-D"${key}=${val}") ;;
            esac
        done < "$f"
    done

    cmake "${cmake_args[@]}" "$PROJECT_ROOT" || {
        if $_egw_submodules_missing; then
            echo "Tip: Try 'submodule-sync' to sync submodules"
        fi
        return 1
    }
    cmake --build "$BUILD_DIR" "$@" || return 1

    # 写入当前架构标记
    mkdir -p "$BUILD_DIR"
    echo "$ARCH" > "${BUILD_DIR}/.build_arch"
}

# 独立运行时自加载配置并执行
if [ -z "${_EGW_LOADED:-}" ]; then
    CONFIG_FILE="${PROJECT_ROOT}/.project.config"
    LOCAL_CONFIG_FILE="${PROJECT_ROOT}/.project.local.config"

    ARCH=x86_64; CMAKE_BUILD_TYPE=Debug
    if [ -f "$LOCAL_CONFIG_FILE" ]; then . "$LOCAL_CONFIG_FILE"; fi
    if [ -f "$CONFIG_FILE" ]; then . "$CONFIG_FILE"; fi
    export ARCH CMAKE_BUILD_TYPE

    _egw_build "$@"; exit $?
fi