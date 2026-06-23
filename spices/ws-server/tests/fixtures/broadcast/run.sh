#!/usr/bin/env bash
# tests/fixtures/broadcast/run.sh -- WS2 live broadcast round-trip runner.
#
# Starts the ws-server broadcast fixture (server.tur) on an ephemeral port,
# then drives it with the ws-client-based client.tur in a separate process (the
# two spices both define WsConn/WsFrame, so they cannot share one program). The
# client opens three concurrent connections, has one send a single text frame,
# and verifies every connection receives it. Exit 0 iff the client does.
#
# Not run by CI's `tur test` (that only picks up flat tests/*.tur); this is the
# manual end-to-end check that exercises the real ws-client <-> ws-server
# broadcast path. The self-contained tests/broadcast_test.tur is the CI cover.
# Usage: tests/fixtures/broadcast/run.sh [path/to/tur]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
TUR="${1:-${TUR:-tur}}"

# Pick a free TCP port without holding it.
PORT="$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')"
export WS_SRV_PORT="$PORT"

cd "$HERE"
"$TUR" run server.tur &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT

# Wait for the listener to accept connections.
for _ in $(seq 1 100); do
  python3 -c "import socket,sys; s=socket.socket(); sys.exit(0 if s.connect_ex(('127.0.0.1',$PORT))==0 else 1)" && break
  sleep 0.1
done

"$TUR" run client.tur
