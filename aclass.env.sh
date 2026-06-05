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
#   submodule-add <url> <path> [tag]  添加 git submodule（可选锁定 tag）
#   submodule-rm <path>          删除 git submodule
#   submodule-sync               同步所有 submodule
#   help               显示此帮助信息

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
export PROJECT_ROOT
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:${PATH}"

CONFIG_FILE="${PROJECT_ROOT}/.project.config"
LOCAL_CONFIG_FILE="${PROJECT_ROOT}/.project.local.config"
SM_FILE="${PROJECT_ROOT}/.project.submodules"

_egw_load_config() {
    ARCH=x86_64
    ACLASS_LIB_MODE=SHARED
    CMAKE_BUILD_TYPE=Debug

    if [ -f "$LOCAL_CONFIG_FILE" ]; then
        # shellcheck source=/dev/null
        . "$LOCAL_CONFIG_FILE"
    fi

    if [ -f "$CONFIG_FILE" ]; then
        # shellcheck source=/dev/null
        . "$CONFIG_FILE"
    fi

    export ARCH CMAKE_BUILD_TYPE

    if [ -n "${COMPILE_PATH:-}" ]; then
        export COMPILE_PATH
    fi
    if [ -n "${SYSROOT_PATH:-}" ]; then
        export SYSROOT_PATH
    fi
}

_egw_init_local_config() {
    if [ ! -f "$LOCAL_CONFIG_FILE" ]; then
        cat > "$LOCAL_CONFIG_FILE" <<'TEMPLATE'
# ── 本地配置 ──────────────────────────────────────────
# 此文件会被 .project.config 的同名变量覆盖，不提交 git
# 目前主要负责提供编译链路径，其余变量不生效
# ──────────────────────────────────────────────────────

# ── 编译器路径（需指定GCC位置时填写）─────────────────────
# COMPILE_PATH=/opt/toolchains/armv7

TEMPLATE
        echo "Created .project.local.config"
    fi
}

_egw_load_config
_egw_init_local_config

if [ ! -f "$SM_FILE" ]; then
    touch "$SM_FILE"
    echo "Created .project.submodules"
fi

# 顶层一次性加载所有函数定义
_EGW_LOADED=true
export _EGW_LOADED

# shellcheck source=/dev/null
. "${PROJECT_ROOT}/scripts/build.sh"
. "${PROJECT_ROOT}/scripts/release.sh"
. "${PROJECT_ROOT}/scripts/clean.sh"
. "${PROJECT_ROOT}/scripts/submodule.sh"

# 函数体：简单调用
build()            { _egw_build; }
release()          { CMAKE_BUILD_TYPE=Release; export CMAKE_BUILD_TYPE; _egw_build; _egw_release; }
clean()            { _egw_clean; }
rebuild()          { _egw_clean; _egw_build; }

run() {
    if [ "$ARCH" = "armv7" ]; then
        echo "Cannot run ARM binary on this host."
        echo "Deploy install/ to target board or set ARCH=x86_64 for local testing."
        return 1
    fi
    "${PROJECT_ROOT}/build/bin/${ACLASS_PROJECT_NAME:-AClassDemo_x86_64}" "$@"
}

submodule-add()    { _egw_submodule_add "$@"; }
submodule-rm()     { _egw_submodule_rm "$@"; }
submodule-sync()   { _egw_submodule_sync; }

_egw_help() {
    echo "Available commands:"
    echo "  build              cmake 配置 + 编译"
    echo "  release            编译 Release 并安装到 install/"
    echo "  clean              清理构建产物"
    echo "  rebuild            clean + build"
    echo "  run                运行网关程序（x86_64 only）"
    echo "  submodule-add      添加 git submodule（可选 tag 锁定版本）"
    echo "  submodule-rm       删除 git submodule"
    echo "  submodule-sync     同步所有 submodule"
    echo "  help               显示此帮助信息"
}

help() { _egw_help; }

_egw_help