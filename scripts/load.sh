#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

echo "Installing from $BUILD_DIR to $INSTALL_PREFIX ..."
cmake --install "$BUILD_DIR" --prefix "$INSTALL_PREFIX"
echo "Done."
