#!/usr/bin/env bash
# shellcheck shell=bash
# scripts/submodule.sh — git submodule 操作

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"

SM_FILE="${PROJECT_ROOT}/.project.submodules"
GITMODULES="${PROJECT_ROOT}/.gitmodules"

# 缓存外部命令路径
_EGW_GIT="$(command -v git 2>/dev/null || echo /usr/bin/git)"
_EGW_RM="$(command -v rm 2>/dev/null || echo /bin/rm)"

# 每次 git 调用都显式传递 PATH，绕过 zsh 继承问题
_GH() { PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH" "$_EGW_GIT" "$@"; }
 
_egw_submodule_add() {
    local url="$1"
    local submod_path="$2"
    local tag="$3"
    local name
    name="${submod_path##*/}"

    # 写 .project.submodules
    _GH config -f "$SM_FILE" "submodule.$name.url" "$url"
    _GH config -f "$SM_FILE" "submodule.$name.path" "$submod_path"
    if [ -n "$tag" ]; then
        _GH config -f "$SM_FILE" "submodule.$name.tag" "$tag"
    fi

    # 用 git submodule add 处理完整注册逻辑
    _GH -C "$PROJECT_ROOT" submodule add --name "$name" "$url" "$submod_path"

    if [ -n "$tag" ]; then
        _GH -C "${PROJECT_ROOT}/${submod_path}" checkout "$tag"
    fi

    echo "Submodule added: $submod_path"
    echo "Remember to commit: git add .project.submodules && git commit"
}

_egw_submodule_rm() {
    local submod_path="$1"
    local name
    name="${submod_path##*/}"

    # 清 .project.submodules
    _GH config -f "$SM_FILE" --remove-section "submodule.$name" 2>/dev/null || true

    # git 处理 submodule 删除
    _GH -C "$PROJECT_ROOT" submodule deinit -f "$submod_path"
    _GH -C "$PROJECT_ROOT" rm -f "$submod_path"
    "$_EGW_RM" -rf "${PROJECT_ROOT}/.git/modules/${name}" "${PROJECT_ROOT}/.git/modules/${submod_path}"
    echo "Submodule removed: $submod_path"
}

_egw_submodule_sync() {
    local entries
    entries=$(_GH config -f "$SM_FILE" --name-only --get-regexp '\.url$' 2>/dev/null || true)

    while IFS= read -r key; do
        [ -z "$key" ] && continue
        local name
        name="${key#submodule.}"
        name="${name%.url}"
        local url path tag
        url=$(_GH config -f "$SM_FILE" "submodule.$name.url")
        path=$(_GH config -f "$SM_FILE" "submodule.$name.path" 2>/dev/null || echo "$name")
        tag=$(_GH config -f "$SM_FILE" "submodule.$name.tag" 2>/dev/null || true)

        if [ ! -d "${PROJECT_ROOT}/${path}" ]; then
            local in_gitmodules
            in_gitmodules=$(_GH config -f "$GITMODULES" "submodule.$name.url" 2>/dev/null || true)
            if [ -n "$in_gitmodules" ]; then
                _GH -C "$PROJECT_ROOT" submodule update --init "$path"
            else
                _GH -C "$PROJECT_ROOT" submodule add --name "$name" "$url" "$path"
            fi
        fi

        if [ -n "$tag" ] && [ -d "${PROJECT_ROOT}/${path}" ]; then
            local current
            current=$(_GH -C "${PROJECT_ROOT}/${path}" describe --tags --exact-match 2>/dev/null || true)
            if [ "$current" != "$tag" ]; then
                echo "Checking out $name @ $tag ..."
                _GH -C "${PROJECT_ROOT}/${path}" checkout "$tag"
            fi
        fi
    done <<< "$entries"

    if [ -d "${PROJECT_ROOT}/third-party" ]; then
        local find_dirs
        find_dirs=$(find "${PROJECT_ROOT}/third-party" -mindepth 1 -maxdepth 1 -type d 2>/dev/null || true)
        while IFS= read -r submod_dir; do
            [ -z "$submod_dir" ] && continue
            local dir_name
            dir_name="${submod_dir##*/}"

            local in_smfile=false
            local entries2
            entries2=$(_GH config -f "$SM_FILE" --name-only --get-regexp '\.url$' 2>/dev/null || true)
            while IFS= read -r key; do
                [ -z "$key" ] && continue
                local n
                n="${key#submodule.}"
                n="${n%.url}"
                if [ "$n" = "$dir_name" ]; then
                    in_smfile=true
                    break
                fi
            done <<< "$entries2"

            if ! $in_smfile; then
                echo -n "Directory '$submod_dir' not in .project.submodules. Delete? [y/N] "
                local confirm
                read -r confirm </dev/tty
                if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]; then
                    _GH -C "$PROJECT_ROOT" submodule deinit -f "$submod_dir"
                    _GH -C "$PROJECT_ROOT" rm -f "$submod_dir"
                    "$_EGW_RM" -rf "${PROJECT_ROOT}/.git/modules/${submod_dir##*/}" "${PROJECT_ROOT}/.git/modules/${dir_name}"
                    echo "Removed: $submod_dir"
                fi
            fi
        done <<< "$find_dirs"
    fi

    echo "Submodules synced."
}

_egw_submodules_missing=false

_egw_check_submodules() {
    _egw_submodules_missing=false
    local sm_entries
    sm_entries=$("$_EGW_GIT" config -f "${PROJECT_ROOT}/.project.submodules" --name-only --get-regexp '\.path$' 2>/dev/null || true)
    while IFS= read -r key; do
        [ -z "$key" ] && continue
        local name
        name="${key#submodule.}"
        name="${name%.path}"
        local path
        path=$("$_EGW_GIT" config -f "${PROJECT_ROOT}/.project.submodules" "submodule.$name.path" 2>/dev/null || true)
        if [ -n "$path" ] && [ ! -d "${PROJECT_ROOT}/${path}" ]; then
            _egw_submodules_missing=true
            break
        fi
    done <<< "$sm_entries"
}