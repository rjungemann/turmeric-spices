#!/usr/bin/env bash
# Compile-fail tests for the typed/linear thread-pool surface.
#
# These .tur files are EXPECTED to be rejected by `tur check`, so they cannot
# live under tests/thread-pool/ (which `tur test` builds and runs as passing
# programs).  Each file's header names the TUR-E#### diagnostic it must raise;
# this runner asserts that exact code appears and that the check fails.
#
# Usage:
#   TUR_BIN=/path/to/tur ./run.sh          # from spices/thread-pool
#   (TUR_BIN defaults to `tur` on PATH, or vendor/tur/tur if present)
set -u

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
TUR="${TUR_BIN:-}"
if [ -z "$TUR" ]; then
  if [ -x "$root/../../vendor/tur/tur" ]; then TUR="$root/../../vendor/tur/tur";
  else TUR="tur"; fi
fi

cases=(
  "forgotten-stop.tur:TUR-E0100"
  "double-stop.tur:TUR-E0101"
  "wrong-item-type.tur:TUR-E0001"
)

fails=0
for c in "${cases[@]}"; do
  file="${c%%:*}"; want="${c##*:}"
  out="$("$TUR" check -I "$root/src" "$here/$file" 2>&1)"
  rc=$?
  if [ $rc -eq 0 ]; then
    echo "FAIL: $file compiled, expected $want"; fails=$((fails+1)); continue
  fi
  if ! printf '%s' "$out" | grep -q "$want"; then
    echo "FAIL: $file rejected but not with $want"; echo "$out" | head -3; fails=$((fails+1)); continue
  fi
  echo "ok: $file -> $want"
done

if [ $fails -eq 0 ]; then echo "PASS compile-fail (${#cases[@]} cases)"; else echo "compile-fail: $fails failure(s)"; fi
exit $fails
