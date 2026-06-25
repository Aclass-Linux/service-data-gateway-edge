#!/bin/bash
# 创建一对虚拟串口（socat），用于本地 Modbus 回环调试
# ttyV0 ←→ ttyV1，双向透传
#
# 用法：
#   ./tools/virtual_serial.sh start   # 启动（后台）
#   ./tools/virtual_serial.sh stop    # 停止
#   ./tools/virtual_serial.sh status  # 检查状态
#   ./tools/virtual_serial.sh restart # 重启
#
# 默认路径 /tmp/ttyV0 ↔ /tmp/ttyV1，可通过环境变量覆盖：
#   LINK0=/path/a LINK1=/path/b ./tools/virtual_serial.sh start

set -e

LINK0="${LINK0:-/tmp/ttyV0}"
LINK1="${LINK1:-/tmp/ttyV1}"
PID_FILE="/tmp/egw_socat.pid"
LOG_FILE="/tmp/egw_socat.log"

start() {
    if status >/dev/null 2>&1; then
        echo "already running (pid $(cat "$PID_FILE"))"
        return 0
    fi

    pkill -x socat 2>/dev/null || true
    rm -f "$LINK0" "$LINK1"

    socat -d PTY,link="$LINK0",raw,echo=0 \
              PTY,link="$LINK1",raw,echo=0 \
              > "$LOG_FILE" 2>&1 &

    local pid=$!
    echo "$pid" > "$PID_FILE"

    # 等待 symlink 就绪
    for _ in $(seq 1 20); do
        if [ -L "$LINK0" ] && [ -L "$LINK1" ]; then
            echo "started (pid $pid): $LINK0 ↔ $LINK1"
            return 0
        fi
        sleep 0.1
    done

    echo "ERROR: socat started but symlinks not ready"
    stop 2>/dev/null || true
    return 1
}

stop() {
    if [ -f "$PID_FILE" ]; then
        local pid
        pid=$(cat "$PID_FILE" 2>/dev/null || echo "")
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
        rm -f "$PID_FILE"
    fi
    pkill -x socat 2>/dev/null || true
    rm -f "$LINK0" "$LINK1"
    echo "stopped"
}

status() {
    if [ -f "$PID_FILE" ]; then
        local pid
        pid=$(cat "$PID_FILE" 2>/dev/null || echo "")
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            echo "running (pid $pid): $LINK0 ↔ $LINK1"
            return 0
        fi
    fi
    echo "not running"
    return 1
}

case "${1:-start}" in
    start)   start   ;;
    stop)    stop    ;;
    status)  status  ;;
    restart) stop; start ;;
    *)
        echo "usage: $0 {start|stop|status|restart}"
        exit 1
        ;;
esac
