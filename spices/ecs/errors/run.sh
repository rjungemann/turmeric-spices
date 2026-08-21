#!/usr/bin/env bash
# errors/run.sh -- assert the ecs compile-FAIL diagnostics.
#
# These are compile-FAIL fixtures: each must be REJECTED, with a specific
# diagnostic code, for a specific reason. That is the inverse of every
# other test in the spice, so they live outside tests/ -- `tur test tests`
# recurses (despite what ci.yml used to claim), and counted all 16 of
# these as suite failures, which made the ecs CI job red on this directory
# alone while the 70+ real tests passed.
#
# Relocating them fixes the red job. This script is the other half: it
# asserts each fixture fails for the RIGHT reason. Checking only "does not
# compile" is how two fixtures silently stopped testing their subject:
#
#   * defsystem-set-undeclared -- drifted onto TUR-E0295 (the `(:: w
#     GameWorld)` bridge) when worlds became by-value aggregates, so it
#     never reached the cap check it exists to prove. Fixed by giving the
#     unsized system surface a typed world (`defsystem-world`); see
#     docs/ecs-unsized-defsystem-cannot-reach-its-world.md.
#   * xworld-unused-world -- drifted onto TUR-E0001 (`expected Slot, got
#     int`) when RE0 lifted storage indices to `Slot`, so the
#     unused-world validation it names was never exercised. Fixed by
#     passing `(slot-new 0)`.
#
# Both had been "passing" the whole time. Hence the WITNESS argument
# below: the code alone is too coarse -- TUR-E0001 and TUR-E0003 each
# cover several fixtures for unrelated reasons -- so every assertion also
# pins a substring of the message that only the intended defect produces.
#
# Usage: errors/run.sh [path/to/tur]
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SPICE_ROOT="$(cd "$HERE/.." && pwd)"
TUR="${1:-${TUR:-tur}}"

cd "$SPICE_ROOT"

fail=0
n=0

# expect_reject <file> <code> <witness> <desc>
#
# Asserts `tur check <file>` reports <code> AND that the diagnostic text
# contains <witness>. The witness is what keeps the fixture pinned to its
# subject rather than to "some error happened".
expect_reject() {
  local file="$1" code="$2" witness="$3" desc="$4"
  local out
  n=$((n + 1))
  out="$("$TUR" check "errors/$file" 2>&1)"

  if ! printf '%s' "$out" | grep -q "error \[$code\]"; then
    echo "not ok $n - $desc"
    if printf '%s' "$out" | grep -q "error \["; then
      echo "    expected $code; got:"
      printf '%s\n' "$out" | grep -m3 -o 'error \[TUR-[EW][0-9]*\]: .*' \
        | sed 's/^/      /'
    else
      echo "    expected $code, but the file compiled clean"
    fi
    fail=1
    return
  fi

  if ! printf '%s' "$out" | grep -qF "$witness"; then
    echo "not ok $n - $desc"
    echo "    $code fired, but not for the expected reason."
    echo "    expected the message to mention: $witness"
    echo "    got:"
    printf '%s\n' "$out" | grep -m3 -o "error \[$code\]: .*" | sed 's/^/      /'
    fail=1
    return
  fi

  echo "ok $n - $desc"
}

echo "# ecs compile-fail diagnostics"

# --- linear write-capability discipline (ecs/cap) ---------------------
expect_reject cap-double-use.tur TUR-E0101 \
  "used after being consumed" \
  "a WriteCap consumed twice is a compile error"
expect_reject cap-mint-wrong-component.tur TUR-E0001 \
  "expected (type-app WriteCap Pos), got (type-app WriteCap Vel)" \
  "a WriteCap<Vel> cannot stand in for a WriteCap<Pos>"

# --- :writes enforcement: writing a component you did not declare -----
expect_reject defsystem-undeclared-write.tur TUR-E0003 \
  "unbound symbol 'Vel-write-cap'" \
  "unsized defsystem: undeclared write has no cap in scope"
expect_reject defsystem-set-undeclared.tur TUR-E0003 \
  "unbound symbol 'Vel-write-cap'" \
  "unsized defsystem-world: undeclared write through the typed accessor"
expect_reject sized-defsystem-undeclared-write.tur TUR-E0003 \
  "unbound symbol 'Vel-write-cap'" \
  "sized defsystem: undeclared write through the typed accessor"
expect_reject xworld-undeclared-write.tur TUR-E0003 \
  "unbound symbol 'ren-RenderPos-write-cap'" \
  "cross-world defxsystem: undeclared write has no cap in scope"

# --- accessor cap gating ----------------------------------------------
expect_reject set-without-cap.tur TUR-E0001 \
  "function 'set-Pos!' arg 2: expected GameWorld, got Slot" \
  "set-Pos! with the cap argument omitted does not elaborate"
expect_reject set-wrong-component.tur TUR-E0001 \
  "expected (type-app WriteCap Pos), got (type-app WriteCap Vel)" \
  "set-Pos! rejects a WriteCap for the wrong component"
expect_reject xworld-wrong-world-cap.tur TUR-E0001 \
  "expected (type-app (type-app XWriteCap RenderWorld) Pos)" \
  "set-Pos! rejects an XWriteCap minted against the wrong world"

# --- cross-world system declaration validation -------------------------
expect_reject xworld-unused-world.tur TUR-E0003 \
  "defxsystem--world-pred-declared-but-never-read-or-written" \
  "defxsystem rejects a world declared but never read or written"

# --- RE0 handle types: Slot / Entity are not interchangeable ints ------
expect_reject int-not-slot.tur TUR-E0001 \
  "expected Slot, got int" \
  "a raw int where a Slot is required does not elaborate"
expect_reject slot-not-entity.tur TUR-E0001 \
  "expected Entity, got Slot" \
  "a Slot where an Entity is required does not elaborate"

# --- sized worlds: cross-parameter size unification --------------------
expect_reject sized-dense-cross-param-reject.tur TUR-E0260 \
  "shares size variable 'n' across parameters" \
  "sized-dense-zip-len rejects mismatched capacities"
expect_reject sized-zip-cross-shape-reject.tur TUR-E0260 \
  "shares size variable 'n' across parameters" \
  "sized-zip-all-three rejects mismatched capacities"

# --- refined worlds: the frozen region and its seal --------------------
expect_reject frozen-despawn-in-region.tur TUR-E0200 \
  "active borrow" \
  "despawning inside a frozen region is locked out by the borrow"
expect_reject refined-world-sealed-alias.tur TUR-E0302 \
  "sealed opaque 'RGWorld'" \
  "a sealed RGWorld cannot be reconstructed through a coercing cast"

echo "# $n compile-fail fixtures checked"
if [ "$fail" -ne 0 ]; then
  echo "FAILED"
  exit 1
fi
echo "# all ecs diagnostics fired as expected"
