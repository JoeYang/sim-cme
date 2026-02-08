#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONFIG="${SCRIPT_DIR}/config/exchange_config.yaml"
LOG="/tmp/exchange.log"
PID_FILE="/tmp/exchange.pid"

usage() {
    echo "Usage: $0 {start|stop|restart|status|log}"
    echo "  start   - Build and start the exchange in the background"
    echo "  stop    - Stop the running exchange"
    echo "  restart - Stop then start"
    echo "  status  - Check if the exchange is running"
    echo "  log     - Tail the exchange log"
    exit 1
}

is_running() {
    if [[ -f "$PID_FILE" ]]; then
        local pid
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
        rm -f "$PID_FILE"
    fi
    return 1
}

do_start() {
    if is_running; then
        echo "Exchange is already running (PID $(cat "$PID_FILE"))"
        exit 0
    fi

    echo "Building sim-cme-exchange (optimized)..."
    bazel build --config=opt //src:sim_cme_exchange 2>&1 | tail -3

    BINARY="${SCRIPT_DIR}/bazel-bin/src/sim_cme_exchange"

    echo "Starting exchange..."
    nohup "$BINARY" --config "$CONFIG" "$@" > "$LOG" 2>&1 &
    local pid=$!
    echo "$pid" > "$PID_FILE"

    sleep 1
    if kill -0 "$pid" 2>/dev/null; then
        echo ""
        echo "Exchange started (PID $pid)"
        echo "  Log: $LOG"
        echo "  TCP: 0.0.0.0:9563"
        echo ""
        echo "  Channels:"
        echo "    310 (ES/MES)  incremental: 239.1.1.1:14310  snapshot: 239.1.1.2:15310"
        echo "    311 (NQ/MNQ)  incremental: 239.1.1.1:14311  snapshot: 239.1.1.2:15311"
        echo "    312 (YM/MYM)  incremental: 239.1.1.1:14312  snapshot: 239.1.1.2:15312"
        echo "    313 (RTY/M2K) incremental: 239.1.1.1:14313  snapshot: 239.1.1.2:15313"
    else
        echo "ERROR: Exchange failed to start. Check $LOG"
        rm -f "$PID_FILE"
        exit 1
    fi
}

do_stop() {
    if ! is_running; then
        echo "Exchange is not running"
        return
    fi

    local pid
    pid=$(cat "$PID_FILE")
    echo "Stopping exchange (PID $pid)..."
    kill "$pid" 2>/dev/null

    # Wait for graceful shutdown
    for i in $(seq 1 10); do
        if ! kill -0 "$pid" 2>/dev/null; then
            rm -f "$PID_FILE"
            echo "Exchange stopped"
            return
        fi
        sleep 0.5
    done

    # Force kill if still running
    echo "Force killing..."
    kill -9 "$pid" 2>/dev/null
    rm -f "$PID_FILE"
    echo "Exchange stopped"
}

do_status() {
    if is_running; then
        echo "Exchange is running (PID $(cat "$PID_FILE"))"
    else
        echo "Exchange is not running"
    fi
}

case "${1:-}" in
    start)   shift; do_start "$@" ;;
    stop)    do_stop ;;
    restart) do_stop; sleep 1; do_start ;;
    status)  do_status ;;
    log)     tail -f "$LOG" ;;
    *)       usage ;;
esac
