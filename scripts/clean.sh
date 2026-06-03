#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"

if [ -d "$BUILD_DIR" ]; then
  echo "Removing $BUILD_DIR ..."
  rm -rf "$BUILD_DIR"
fi
echo "Done."
