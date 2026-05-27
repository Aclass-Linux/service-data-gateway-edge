#!/usr/bin/env bash
# source this file:  source scripts/env.sh
# Or add to ~/.zshrc:  source /path/to/scripts/env.sh

# 不使用 set -e，防止 source 后编译失败退终端

# portable: works in both bash and zsh
if [ -n "${BASH_SOURCE:-}" ]; then
  _self="${BASH_SOURCE[0]}"
else
  _self="$0"
fi
PROJECT_ROOT="$(cd "$(dirname "$_self")/.." && pwd)"

# ---------- load config file (env vars win over config) ----------
if [ -f "${PROJECT_ROOT}/.project-config" ]; then
  . "${PROJECT_ROOT}/.project-config"
fi

# ---------- default settings ----------
: "${ARCH:=x86}"
: "${BUILD_TYPE:=debug}"
: "${BUILD_DIR:=${PROJECT_ROOT}/build}"
: "${CMAKE_ARGS:=}"

_nproc() {
  if command -v nproc &>/dev/null; then
    nproc
  else
    echo 4
  fi
}

__cmake_gen() {
  local build_dir="$1"
  local extra=()
  if [ "$ARCH" = "arm32" ]; then
    extra+=(-DCMAKE_TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/arm32-toolchain.cmake")
  fi
  cmake -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$([ "$BUILD_TYPE" = release ] && echo Release || echo Debug)" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ${CMAKE_ARGS} \
    "${extra[@]}"
}

# ---------- commands ----------
build() {
  echo ":: building  arch=${ARCH} type=${BUILD_TYPE}"
  __cmake_gen "${BUILD_DIR}" || { echo "!! cmake configure failed"; return 1; }
  cmake --build "${BUILD_DIR}" -j"$(_nproc)" "$@" || { echo "!! build failed"; return 1; }
}

clean() {
  echo ":: cleaning  ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
}

rebuild() {
  clean
  build
}

run() {
  local exe="${PROJECT_ROOT}/out/bin/data-gateway-edge"
  if [ ! -f "$exe" ]; then
    echo ":: binary not found, building first"
    build || return 1
  fi
  echo ":: running  ${exe}"
  "${exe}" "$@"
}

test() {
  __cmake_gen "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" -j"$(_nproc)"
  ctest --test-dir "${BUILD_DIR}" --output-on-failure "$@"
}

sync() {
  echo ":: sync  (placeholder — add git pull / submodule update / rsync here)"
  git -C "${PROJECT_ROOT}" pull --ff-only
}

help() {
  echo "Usage: source env.sh  then use any of:"
  echo "  build              compile (arch=\$ARCH, type=\$BUILD_TYPE)"
  echo "  clean              remove build directory"
  echo "  rebuild            clean + build"
  echo "  run [args...]      run the compiled binary"
  echo "  test [args...]     run tests via ctest"
  echo "  sync               git pull --ff-only"
  echo ""
  echo "  export ARCH=arm32        (default: x86)"
  echo "  export BUILD_TYPE=release (default: debug)"
}

echo "[env] sourced. Available: build | clean | rebuild | run | test | sync | help"
echo "      ARCH=${ARCH}  BUILD_TYPE=${BUILD_TYPE}"
echo "      \$ source scripts/env.sh   or add it to ~/.zshrc"
