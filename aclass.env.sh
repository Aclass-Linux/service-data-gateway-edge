# shellcheck shell=sh
# 用法：source env.sh
#
# 提供以下命令：
#   build     配置并编译
#   clean     清理构建产物
#   install   安装到系统路径
#   rebuild   先 clean 再 build
#   run       运行网关程序

BUILD_DIR="${BUILD_DIR:-build}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

build() {
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
    cmake --build "$BUILD_DIR" "$@"
}

clean() {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        echo "Removed $BUILD_DIR"
    fi
}

install_() {
    cmake --install "$BUILD_DIR" --prefix "$INSTALL_PREFIX"
}

rebuild() {
    clean
    build "$@"
}

run() {
    "$BUILD_DIR/src/app/gateway" "$@"
}
