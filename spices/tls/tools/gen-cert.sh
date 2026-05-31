#!/usr/bin/env bash
# tools/gen-cert.sh -- emit a self-signed cert + private key into a target
# directory. Used by smoke-test fixtures so they can exercise the full
# tls-ctx-load-cert-pem / tls-ctx-load-key-pem path without depending on
# a real PKI. The cert is throwaway: 1-day validity, CN=localhost,
# 2048-bit RSA. Do not use for anything real.
#
# Usage: tools/gen-cert.sh <out-dir>
#   Writes:  <out-dir>/test-cert.pem
#            <out-dir>/test-key.pem
set -euo pipefail

OUT_DIR="${1:-}"
if [ -z "$OUT_DIR" ]; then
    echo "usage: $0 <out-dir>" >&2
    exit 2
fi
mkdir -p "$OUT_DIR"

CERT="$OUT_DIR/test-cert.pem"
KEY="$OUT_DIR/test-key.pem"

# -nodes: write the key unencrypted (test-only); the spice does not
# implement password-protected key loading in v0.1.0.
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$KEY" -out "$CERT" \
    -days 1 -subj "/CN=localhost" \
    >/dev/null 2>&1

echo "$CERT"
echo "$KEY"
