#!/usr/bin/env bash
# send_orders.sh - Send test orders to the CME exchange simulator
# Usage: ./send_orders.sh [--host HOST] [--port PORT] [--pace SECONDS] [--rounds N]

set -e

HOST="127.0.0.1"
PORT="9563"
PACE="1"       # seconds between orders
ROUNDS="10"    # number of rounds (each round = ~8 orders)
CLIENT="$(dirname "$0")/bazel-bin/tools/ilink3_client"

while [[ $# -gt 0 ]]; do
  case $1 in
    --host)  HOST="$2"; shift 2;;
    --port)  PORT="$2"; shift 2;;
    --pace)  PACE="$2"; shift 2;;
    --rounds) ROUNDS="$2"; shift 2;;
    --help|-h)
      echo "Usage: $0 [--host HOST] [--port PORT] [--pace SECONDS] [--rounds N]"
      echo "  --host    Exchange host (default: 127.0.0.1)"
      echo "  --port    Exchange port (default: 9563)"
      echo "  --pace    Seconds between orders (default: 1)"
      echo "  --rounds  Number of rounds to run (default: 10)"
      exit 0;;
    *) echo "Unknown option: $1"; exit 1;;
  esac
done

if [[ ! -x "$CLIENT" ]]; then
  echo "Error: ilink3_client not found at $CLIENT"
  echo "Build it first: bazel build --config=opt //tools:ilink3_client"
  exit 1
fi

PIPE=$(mktemp -u /tmp/order_pipe.XXXXXX)
mkfifo "$PIPE"
trap "rm -f $PIPE" EXIT

# Start client
"$CLIENT" --host "$HOST" --port "$PORT" --interactive < "$PIPE" &
CLIENT_PID=$!
trap "rm -f $PIPE; kill $CLIENT_PID 2>/dev/null" EXIT

exec 3>"$PIPE"
sleep 2

echo "=== Sending orders: $ROUNDS rounds at ${PACE}s pace ==="
echo ""

for round in $(seq 1 "$ROUNDS"); do
  echo "--- Round $round/$ROUNDS ---"

  # Build depth on ESH5 (SecID=1)
  echo "buy 1 5 4999.00" >&3; sleep "$PACE"
  echo "buy 1 10 4999.25" >&3; sleep "$PACE"
  echo "sell 1 8 5000.75" >&3; sleep "$PACE"
  echo "sell 1 10 5001.00" >&3; sleep "$PACE"

  # Cross: aggressive sell sweeps bids
  echo "sell 1 8 4999.00" >&3; sleep "$PACE"
  # Cross: aggressive buy sweeps asks
  echo "buy 1 12 5001.00" >&3; sleep "$PACE"

  # Activity on NQH5 (SecID=2)
  echo "buy 2 3 17800.00" >&3; sleep "$PACE"
  echo "sell 2 3 17800.00" >&3; sleep "$PACE"
done

# Print stats
sleep 2
echo "status" >&3
sleep 1

echo ""
echo "=== Done: $ROUNDS rounds completed ==="
echo "quit" >&3
exec 3>&-

wait $CLIENT_PID 2>/dev/null
