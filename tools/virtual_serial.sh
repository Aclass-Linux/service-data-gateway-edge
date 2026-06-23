#!/bin/bash
# 创建一对虚拟串口（socat），用于本地调试
# ttyV0 ←→ ttyV1，双向透传

set -e

LINK0="${1:-/tmp/ttyV0}"
LINK1="${2:-/tmp/ttyV1}"

cleanup() {
    rm -f "$LINK0" "$LINK1"
}
trap cleanup EXIT

# 清理已存在的 socat 实例和链接
pkill -x socat 2>/dev/null || true
rm -f "$LINK0" "$LINK1"

exec socat -d -d PTY,link="$LINK0",raw,echo=0 \
              PTY,link="$LINK1",raw,echo=0
