#!/usr/bin/env bash
# tests/fixtures/tls-roundtrip/run.sh -- T3 smoke runner.
#
# Generates a fresh self-signed cert + key under /tmp/tls-smoke-test/
# (shared with the ctx-lifecycle fixture), compiles main.tur with
# auto-spice discovery, and runs the resulting program. The program
# spawns a server thread that uses the spice's public API and a client
# (inline mbedTLS) that exchanges one record. Exit 0 iff the echo round
# trips intact.
#
# Usage: tests/fixtures/tls-roundtrip/run.sh [path/to/tur]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SPICE_ROOT="$(cd "$HERE/../../.." && pwd)"
TUR="${1:-${TUR:-tur}}"

CERT_DIR=/tmp/tls-smoke-test
"$SPICE_ROOT/tools/gen-cert.sh" "$CERT_DIR" >/dev/null

cd "$HERE"
"$TUR" run main.tur
