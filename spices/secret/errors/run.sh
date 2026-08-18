#!/usr/bin/env bash
# errors/run.sh -- assert the linear-Secret diagnostics.
#
# These are compile-FAIL fixtures: each must be rejected, with a specific
# diagnostic code. That is the inverse of every other test in the spice, so
# they live outside tests/ -- `tur test tests` recurses, and would try to
# build and run them as an ordinary suite.
#
# This is the half of the design that a passing program cannot demonstrate.
# secret/core's whole claim is that forgetting to wipe key material is a
# compile-time error; the only way to show that is to compile something that
# forgets, and check the compiler said no. A fixture that merely *sat* in the
# tree unverified (the tls spice's errors/ are manual) would silently stop
# proving anything the day the diagnostic changed.
#
# Note the discipline is on by DEFAULT as of tur 0.35 -- `-Xsubstructural`
# now warns TUR-W0050 as a no-op. No flag is passed here.
#
# Usage: errors/run.sh [path/to/tur]
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SPICE_ROOT="$(cd "$HERE/.." && pwd)"
TUR="${1:-${TUR:-tur}}"

cd "$SPICE_ROOT"

fail=0

expect_reject() {
  local file="$1" code="$2" desc="$3"
  local out status
  out="$("$TUR" check "$file" 2>&1)"
  status=$?

  if [ "$status" -eq 0 ] && ! printf '%s' "$out" | grep -q "error \[$code\]"; then
    echo "not ok - $desc"
    echo "    expected $code, but the file compiled clean"
    fail=1
    return
  fi
  if ! printf '%s' "$out" | grep -q "error \[$code\]"; then
    echo "not ok - $desc"
    echo "    expected $code; got:"
    printf '%s\n' "$out" | sed 's/^/      /' | head -5
    fail=1
    return
  fi
  echo "ok - $desc"
}

echo "# linear Secret diagnostics"
expect_reject errors/secret-leak-no-wipe.tur   TUR-E0100 \
  "a Secret that is never wiped is a compile error"
expect_reject errors/secret-use-after-wipe.tur TUR-E0101 \
  "reading a Secret after wiping it is a compile error"
expect_reject errors/secret-double-wipe.tur    TUR-E0101 \
  "wiping a Secret twice is a compile error"

if [ "$fail" -ne 0 ]; then
  echo "FAILED"
  exit 1
fi
echo "# all linear-Secret diagnostics fired as expected"
