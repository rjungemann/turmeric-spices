#!/usr/bin/env bash
# bench/run.sh -- the ECS integrate benchmark.
#
# The comparison the archived `ecs-spice-plan.md` specified and never
# wrote: 100k entities with dense float Pos/Vel, integrated
# Pos += Vel * dt for 100 frames, against a hand-rolled C baseline.
# Target: within 2x of hand-rolled.
#
# Every variant computes the SAME arithmetic and prints the same scaled
# checksum; run.sh refuses to report a timing whose checksum disagrees
# with the C baseline. That is what keeps a "fast" row from being a row
# whose loop the optimiser deleted.
#
# Rows:
#   c-baseline    hand-rolled C, two flat arrays          -- the denominator
#   manual        raw Turmeric, flat buffers, no ECS      -- isolates codegen cost
#   ecs-unsized   defworld + for-each2                    -- the headline number
#   ecs-sized     sized-defworld + sized-for-each         -- static bound, RE2 input
#   poly-dispatch (HasPos W) with lookups in the loop     -- ECB input
#   poly-hoisted  ...with the lookups hoisted             -- ECB control
#
# Reading the table:
#   ecs-unsized / manual  = what the ECS abstraction costs
#   manual / c-baseline   = what Turmeric's codegen costs on the same loop
#   poly-dispatch / poly-hoisted = whether dictionary dispatch is visible
#
# Usage: bench/run.sh [path/to/tur] [reps]
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SPICE_ROOT="$(cd "$HERE/.." && pwd)"
TUR="${1:-${TUR:-tur}}"
REPS="${2:-5}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

cd "$SPICE_ROOT"

CC="${CC:-cc}"

# Timings are MICROseconds internally and printed as ms with one decimal.
# Millisecond resolution was not enough: the C baseline runs in ~4-5ms, so
# integer-ms rounding alone swung the headline ratio between 7.8x and 9.75x
# across runs. The ratio is the number people quote; it has to be stable.
#
# Best-of-REPS. Wall-clock minimum is the right statistic here: the
# workload is deterministic and every source of variance (scheduler
# preemption, frequency scaling, a co-scheduled build) can only ADD time.
# Reporting a mean would fold that noise into the number.
best_of() {
  local cmd="$1" best="" us="" sum=""
  local i out
  for ((i = 0; i < REPS; i++)); do
    out="$($cmd 2>/dev/null | tail -2)"
    us="$(printf '%s' "$out" | head -1)"
    sum="$(printf '%s' "$out" | tail -1)"
    case "$us" in ''|*[!0-9]*) continue ;; esac
    if [ -z "$best" ] || [ "$us" -lt "$best" ]; then best="$us"; fi
  done
  printf '%s %s' "${best:-FAIL}" "${sum:-FAIL}"
}

echo "# ECS integrate benchmark -- 100k entities, 100 frames, best of $REPS"
echo

# --- C baseline --------------------------------------------------------
$CC -O2 -o "$OUT/baseline" bench/integrate-baseline.c || {
  echo "could not build the C baseline; aborting"; exit 1; }
read -r base_us base_sum <<<"$(best_of "$OUT/baseline")"
if [ "$base_us" = "FAIL" ]; then echo "C baseline did not run"; exit 1; fi

fmt_ms() { awk -v u="$1" 'BEGIN{printf "%.1f", u/1000}'; }

printf '%-16s %10s %10s  %s\n' VARIANT MS "VS C" CHECKSUM
printf '%-16s %10s %10s  %s\n' c-baseline "$(fmt_ms "$base_us")" "1.00x" "$base_sum"

fail=0
row() {
  local name="$1" src="$2"
  local us sum ratio
  read -r us sum <<<"$(best_of "$TUR run $src")"
  if [ "$us" = "FAIL" ]; then
    printf '%-16s %10s %10s  %s\n' "$name" "FAIL" "-" "did not run"
    fail=1
    return
  fi
  if [ "$sum" != "$base_sum" ]; then
    printf '%-16s %10s %10s  %s\n' "$name" "$(fmt_ms "$us")" "-" "CHECKSUM MISMATCH ($sum)"
    fail=1
    return
  fi
  # Ratio to two decimals without bc.
  if [ "$base_us" -eq 0 ]; then ratio="n/a"
  else ratio="$(awk -v a="$us" -v b="$base_us" 'BEGIN{printf "%.2fx", a/b}')"
  fi
  printf '%-16s %10s %10s  %s\n' "$name" "$(fmt_ms "$us")" "$ratio" "$sum"
}

row manual        bench/integrate-manual.tur
row ecs-unsized   bench/integrate-ecs.tur
row ecs-sized     bench/integrate-sized.tur
row poly-dispatch bench/integrate-poly.tur
row poly-hoisted  bench/integrate-poly-hoisted.tur

echo
if [ "$fail" -ne 0 ]; then
  echo "# one or more variants failed or disagreed with the baseline checksum"
  exit 1
fi
echo "# all variants agree with the C baseline checksum ($base_sum)"
