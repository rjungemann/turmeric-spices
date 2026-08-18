#!/usr/bin/env bash
# tests/o2/run.sh -- exercise the hygiene floor under -O2.
#
# The default `tur test` build uses whatever flags the driver picks. The
# security-relevant claims of secret/hygiene are all claims about what the
# *optimizer* is allowed to do, so they need a build that actually turns the
# optimizer on. TUR_CC_FLAGS overrides the driver's default cc flags.
#
# Two checks:
#   1. the whole functional suite still passes at -O2 (catches an -O2-only
#      miscompile of the volatile-fnptr wipe or the asm memory barrier);
#   2. tests/o2/residue.tur -- the stack dead-store probe, which is the only
#      check that can actually observe an elided wipe. See its header.
#
# Usage: tests/o2/run.sh [path/to/tur]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SPICE_ROOT="$(cd "$HERE/../.." && pwd)"
TUR="${1:-${TUR:-tur}}"

export TUR_CC_FLAGS="-O2 -std=c99 -Wall -fno-strict-aliasing"
echo "== TUR_CC_FLAGS=$TUR_CC_FLAGS =="

cd "$SPICE_ROOT"

echo
echo "== functional suite at -O2 =="
"$TUR" test tests

echo
echo "== dead-store elimination probe =="
# `tur run` propagates the program's exit status; residue.tur exits 2 when
# key material survived the wipe.
"$TUR" run tests/o2/residue.tur
