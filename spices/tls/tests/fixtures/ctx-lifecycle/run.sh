#!/usr/bin/env bash
# tests/fixtures/ctx-lifecycle/run.sh -- T2 smoke runner.
#
# Generates a fresh self-signed cert + key in /tmp/tls-smoke-test/,
# compiles tests/fixtures/ctx-lifecycle/main.tur with auto-spice
# discovery walking up to spices/tls/build.tur, runs the resulting
# program, and asserts the program exits 0.
#
# Usage: tests/fixtures/ctx-lifecycle/run.sh [path/to/tur]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SPICE_ROOT="$(cd "$HERE/../../.." && pwd)"
TUR="${1:-${TUR:-tur}}"

CERT_DIR=/tmp/tls-smoke-test
"$SPICE_ROOT/tools/gen-cert.sh" "$CERT_DIR" >/dev/null

# tur run auto-discovers spices/tls/build.tur from main.tur's directory
# and pulls in the spice's src/ + cmake-deps; no -I flags required.
cd "$HERE"
"$TUR" run main.tur
