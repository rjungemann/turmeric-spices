#!/usr/bin/env bash
# tests/fixtures/ws-echo-text/run.sh -- WC1/WC3 live smoke runner.
#
# Starts the pure-Python WebSocket echo server on an ephemeral port, exports
# that port as WS_ECHO_PORT, compiles main.tur with auto-spice discovery, and
# runs it. The program connects over plain ws://, round-trips two text frames,
# checks the receive-timeout path, and closes. Exit 0 iff the program does.
#
# Not run by CI's `tur test` (that only picks up flat tests/*.tur); this is the
# manual end-to-end check. Usage: tests/fixtures/ws-echo-text/run.sh [path/to/tur]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
TUR="${1:-${TUR:-tur}}"

# Pick a free TCP port without binding it long-term.
PORT="$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')"
export WS_ECHO_PORT="$PORT"

python3 "$HERE/echo_server.py" "$PORT" &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT

# Give the listener a moment to come up.
for _ in $(seq 1 50); do
  python3 -c "import socket,sys; s=socket.socket(); sys.exit(0 if s.connect_ex(('127.0.0.1',$PORT))==0 else 1)" && break
  sleep 0.1
done

cd "$HERE"
"$TUR" run main.tur
