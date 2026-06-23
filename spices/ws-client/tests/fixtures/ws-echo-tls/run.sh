#!/usr/bin/env bash
# tests/fixtures/ws-echo-tls/run.sh -- WC2 / TLS-V0 live wss:// smoke runner.
#
# Generates a throwaway self-signed cert, starts the shared echo server in TLS
# mode on an ephemeral port (WS_ECHO_PORT), compiles main.tur with auto-spice
# discovery, and runs it over wss://. main.tur exercises all three TLS-V0
# connect entry points: ws-connect-with-ca (verifying against the cert, which
# is its own CA), ws-connect (default system store -- must reject the
# self-signed cert), and ws-connect-insecure (opt-out). WS_TLS_CERT is the CA
# bundle for the verified case. Requires the spice to be built with mbedTLS (so
# `tur fetch` must have run); without it ws-connect returns a null handle and
# the program bails. Not run by CI's `tur test`.
#
# Usage: tests/fixtures/ws-echo-tls/run.sh [path/to/tur]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SHARED="$(cd "$HERE/../ws-echo-text" && pwd)"   # shared echo_server.py
TUR="${1:-${TUR:-tur}}"

CERT_DIR="$(mktemp -d)"
trap 'rm -rf "$CERT_DIR"; kill "${SERVER_PID:-}" 2>/dev/null || true' EXIT
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -subj "/CN=localhost" \
  -keyout "$CERT_DIR/key.pem" -out "$CERT_DIR/cert.pem" >/dev/null 2>&1

PORT="$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')"
export WS_ECHO_PORT="$PORT" WS_TLS_CERT="$CERT_DIR/cert.pem" WS_TLS_KEY="$CERT_DIR/key.pem"

python3 "$SHARED/echo_server.py" "$PORT" &
SERVER_PID=$!
sleep 0.5    # TLS server: give the listener a moment (no plaintext probe)

cd "$HERE"
"$TUR" run main.tur
