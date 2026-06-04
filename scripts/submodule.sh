#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/submodule.sh — git submodule 操作

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"

_dgh_submodule_add() {
    local url="$1"
    local path="$2"
    command git -C "$PROJECT_ROOT" submodule add "$url" "$path"
    echo "Submodule added: $path"
    echo "Remember to: cd $path && git checkout <tag> && cd .. && git commit"
}

_dgh_submodule_rm() {
    local path="$1"
    command git -C "$PROJECT_ROOT" submodule deinit -f "$path"
    command git -C "$PROJECT_ROOT" rm -f "$path"
    command rm -rf "${PROJECT_ROOT}/.git/modules/${path}"
    echo "Submodule removed: $path"
}

_dgh_submodule_sync() {
    command git -C "$PROJECT_ROOT" submodule update --init --recursive
    echo "Submodules synced."
}