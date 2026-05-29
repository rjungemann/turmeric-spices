#!/usr/bin/env bash
# tests/run-fixtures.sh -- run every tur-template fixture and report TAP.
#
# Usage: bash tests/run-fixtures.sh
#
# Each fixture lives in tests/fixtures/<name>/ with main.tur as the entry
# point and emits a single TAP line. The script forwards each fixture's
# output verbatim, then exits non-zero if any fixture failed.

set -u

TUR_BIN="${TUR_BIN:-/Users/rjungemann/Projects/turmeric/build/tur}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

fixtures=(basic if-else for-loop escaping)
fails=0

echo "1..${#fixtures[@]}"

n=0
for f in "${fixtures[@]}"; do
  n=$((n + 1))
  main="${ROOT}/tests/fixtures/${f}/main.tur"
  if [ ! -f "${main}" ]; then
    echo "not ok ${n} - ${f} (missing main.tur)"
    fails=$((fails + 1))
    continue
  fi
  out=$("${TUR_BIN}" run "${main}" 2>/dev/null | tail -1)
  case "${out}" in
    "ok "*) echo "ok ${n} - ${f}" ;;
    *)      echo "not ok ${n} - ${f}"; fails=$((fails + 1)) ;;
  esac
done

if [ "${fails}" = "0" ]; then
  echo "# All ${#fixtures[@]} fixtures passed." >&2
else
  echo "# ${fails} of ${#fixtures[@]} fixtures failed." >&2
  exit 1
fi
