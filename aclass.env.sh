#!/usr/bin/env bash
# shellcheck shell=bash
# aclass.env.sh — DataGatewayHub 构建环境
# 用法：source aclass.env.sh
#
# 提供以下命令：
#   build                cmake 配置 + 编译
#   release              编译 Release 并安装到 install/
#   clean                清理构建产物
#   rebuild              先 clean 再 build
#   run                  运行网关程序（x86_64 only）
#   submodule-add <url> <path>   添加 git submodule
#   submodule-rm <path>          删除 git submodule
#   submodule-sync               同步所有 submodule
#   help               显示此帮助信息

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
export PROJECT_ROOT

CONFIG_FILE="${PROJECT_ROOT}/.project.config"
LOCAL_CONFIG_FILE="${PROJECT_ROOT}/.project.local.config"

_dgh_load_config() {
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

    if [ -n "${CROSS_COMPILE_PATH:-}" ]; then
        export CROSS_COMPILE_PATH
    fi
    if [ -n "${SYSROOT_PATH:-}" ]; then
        export SYSROOT_PATH
    fi
}

_dgh_init_local_config() {
    if [ ! -f "$LOCAL_CONFIG_FILE" ]; then
        cat > "$LOCAL_CONFIG_FILE" <<'TEMPLATE'
# 本地配置（不提交 git）
# 交叉编译时填写以下路径：
# CROSS_COMPILE_PATH=/opt/toolchains/gcc-arm-9.2-2019.12-x86_64-arm-linux-gnueabihf/bin
# SYSROOT_PATH=/opt/sysroots/armv7
TEMPLATE
        echo "Created .project.local.config from template."
        echo "For cross-compilation add toolchain paths:"
        echo "  CROSS_COMPILE_PATH=/path/to/your/toolchain/bin"
        echo "  SYSROOT_PATH=/path/to/your/sysroot"
    fi
}

_dgh_load_config
_dgh_init_local_config

build() {
    # shellcheck source=/dev/null
    . "${PROJECT_ROOT}/scripts/toolchain.sh"
    # shellcheck source=/dev/null
    . "${PROJECT_ROOT}/scripts/build.sh"
}

release() {
    CMAKE_BUILD_TYPE=Release
    export CMAKE_BUILD_TYPE
    build
    # shellcheck source=/dev/null
    . "${PROJECT_ROOT}/scripts/release.sh"
}

clean() {
    # shellcheck source=/dev/null
    . "${PROJECT_ROOT}/scripts/clean.sh"
}

rebuild() {
    clean
    build
}

run() {
    if [ "$ARCH" = "armv7" ]; then
        echo "Cannot run ARM binary on this host."
        echo "Deploy install/ to target board or set ARCH=x86_64 for local testing."
        return 1
    fi
    "${PROJECT_ROOT}/build/bin/gateway" "$@"
}

submodule-add() {
    # shellcheck source=/dev/null
    . "${PROJECT_ROOT}/scripts/submodule.sh"
    _dgh_submodule_add "$@"
}

submodule-rm() {
    # shellcheck source=/dev/null
    . "${PROJECT_ROOT}/scripts/submodule.sh"
    _dgh_submodule_rm "$@"
}

submodule-sync() {
    # shellcheck source=/dev/null
    . "${PROJECT_ROOT}/scripts/submodule.sh"
    _dgh_submodule_sync
}

_dgh_help() {
    echo "Available commands:"
    echo "  build              cmake 配置 + 编译"
    echo "  release            编译 Release 并安装到 install/"
    echo "  clean              清理构建产物"
    echo "  rebuild            clean + build"
    echo "  run                运行网关程序（x86_64 only）"
    echo "  submodule-add      添加 git submodule"
    echo "  submodule-rm       删除 git submodule"
    echo "  submodule-sync     同步所有 submodule"
    echo "  help               显示此帮助信息"
}

help() {
    _dgh_help
}

_dgh_help