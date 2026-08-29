#!/usr/bin/env bash
# fixtures/run.sh -- run every tur-template fixture and report TAP.
#
# These are runnable end-to-end fixtures, not test suites: each is a whole
# program with a `main` that renders a template and prints one TAP line. They
# live outside tests/ because `tur test` recurses -- it would build each of
# them as an ordinary suite, find no tests in it, and count it as a failure.
# That is what made the template job red on this directory alone.
#
# ci.yml runs this via its "Run end-to-end fixtures" step, which is a no-op
# for any spice without an executable fixtures/run.sh.
#
# Usage: fixtures/run.sh [path/to/tur]      # from spices/template
#        TUR_BIN=... fixtures/run.sh
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TUR_BIN="${1:-${TUR_BIN:-tur}}"

# Each fixture's main.tur names its template and expected-output files by a
# path relative to the spice root, so run them from there.
cd "$ROOT" || exit 2

fixtures=(basic if-else for-loop escaping)
fails=0

echo "1..${#fixtures[@]}"

n=0
for f in "${fixtures[@]}"; do
  n=$((n + 1))
  main="fixtures/${f}/main.tur"
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
  echo "# All ${#fixtures[@]} fixtures passed."
else
  echo "# ${fails} of ${#fixtures[@]} fixtures failed."
  exit 1
fi
