# tur-ecs integrate benchmark

The comparison the archived `ecs-spice-plan.md` specified and left unwritten
("**Bench** -- still TODO; the original 100k-entity dense `Pos`/`Vel`
integration vs. hand-rolled comparison has not been written.
Within-2x-of-hand-rolled is the target.").

```sh
bench/run.sh /path/to/tur [reps]
```

Workload: 100,000 entities carrying dense float `Pos` and `Vel`, integrated
`Pos += Vel * dt` for 100 frames (10M component updates), then summed.
`dt = 0.25` and `Vel = (1.5, -0.5)` -- fractional on purpose, so a codegen
path that truncates would move the checksum instead of quietly passing.

Every variant prints the same scaled checksum and `run.sh` refuses to report a
timing that disagrees with the C baseline's. That is what keeps a "fast" row
from being a row whose loop the optimizer deleted.

## Results

Apple M2, 8 cores, macOS 27.0, Apple clang 21.0.0, `tur` v0.37.0, best of 9.

| variant | ms | vs C | what it isolates |
|---|---|---|---|
| `c-baseline` | 4.4 | 1.00x | hand-rolled C, two flat arrays |
| `manual` | 4.6 | 1.04x | raw Turmeric, flat buffers, no ECS |
| `ecs-unsized` | 36.9 | 8.42x | `defworld` + `for-each2` |
| `ecs-sized` | 15.3 | 3.50x | `sized-defworld` + `sized-for-each` |
| `poly-dispatch` | 37.4 | 8.56x | `(HasPos W)` with the lookups in the loop |
| `poly-hoisted` | 37.3 | 8.53x | ...with the lookups hoisted |

Run-to-run the ECS ratios move by about +/-0.3x; the ordering and every
conclusion below are stable across repeated runs.

> Timings are microseconds internally, printed as ms. This matters more than
> it sounds: an earlier revision timed in whole milliseconds, and against a
> ~4.5ms baseline the integer rounding alone moved the headline ratio between
> 7.8x and 9.75x, and manufactured an apparent 3ms gap between the two `poly`
> rows that does not exist. If you change the timing code, do not go back to
> millisecond resolution.

## What this says

**The 2x target is missed, by a lot.** `ecs-unsized` is 8.4x hand-rolled and
`ecs-sized` is 3.5x. Neither is close to the plan's within-2x goal. That is
the headline and it should not be softened: this is the first time the number
has been measured, and it is worse than the plan assumed.

**It is not the backend.** `manual` -- a Turmeric loop over flat buffers,
same arithmetic -- ties the C baseline at 1.04x. So essentially all of the gap
is the ECS abstraction, not Turmeric's codegen. Any future work aimed at
"making Turmeric faster here" is aimed at the wrong layer.

**It is not the query macro either.** `poly-hoisted` is a hand-written `while`
loop over `dense-get`/`dense-set!` with no `for-each` anywhere, and it lands
within noise of `ecs-unsized` (8.53x vs 8.42x) -- despite `for-each2` doing
strictly *more* work per slot (the runtime `__fe-min-cap` probe, the per-slot
`dense-has?` intersection). So the iteration machinery is not what costs; the
storage accessors are.

**Reading `ecs/storage.tur` says why, and it is not a bounds check.** Every
`dense-set!` carries, per write: an `elem_sz` initialization test, a capacity
test guarding a `realloc` auto-grow path, the store itself, a *second* array
write to `present[idx]`, and a `len` update branch. `dense-get` by contrast is
a bare indexed load. The write path is doing four things around the one thing
the C baseline does. `ecs-sized` being 2.4x faster than `ecs-unsized` is
consistent with this -- a sized storage's capacity is static, so the grow
branch has nothing to do.

This is measured (the timings, the checksum agreement, the `poly-hoisted` ==
`ecs-unsized` tie) plus read from the source (what `dense-set!` emits). The
per-cost attribution *within* `dense-set!` is not separately measured; a
follow-up that ablates the auto-grow branch and the `present[]` write
independently would settle it, and is the obvious next probe.

Also not isolated: `ecs-sized` still writes a `present[]` byte and does a
struct-by-value copy per store, so its 3.5x is not attributable to the grow
branch alone either. The 8.4x -> 3.5x delta is what removing a *static*
capacity buys in total, not a decomposition.

## Consequences for the two deferred plans

Both of these were explicitly gated on "a profile". This is the profile.

### ECB -- the structural `(has ...)` bound: no case

`poly-dispatch` (2 x 10M dictionary lookups) vs `poly-hoisted` (2 total)
is **37.4ms vs 37.3ms** -- a 0.3% difference, i.e. nothing. Twenty million
dictionary lookups cost no measurable time at all; `cc -O2` hoists the
dispatch out of the loop on its own, so the source-level hoist changes
nothing.

`docs/upcoming/v1/ecs-component-set-bounds-plan.md` says the feature "does not
start without a profile showing dictionary dispatch in an ECS hot path". This
profile shows the opposite as strongly as it can be shown: the indirection the
whole feature exists to remove is already gone before it reaches the CPU. The
plan's own precedent -- the whole-program entry-check elision that measured at
**zero** because `cc -O2` had already done it -- is exactly what happened
again.

(An earlier revision of this file reported a ~3ms gap here. That was
millisecond rounding, not dispatch; see the timing note above.)

### RE2 -- bounded slot indices: plausible, but aimed slightly off

RE2 wants to discharge `0 <= i < cap` statically so dense storage can drop its
per-access check. There *is* a per-write capacity test to remove, so the idea
has a real target -- but it is an auto-grow guard, not a pure bounds check,
and removing it leaves the `present[]` write and the `len` update behind. The
`ecs-sized` row is the closest thing to an upper bound on the win: a world
whose capacity is already static, with the grow branch already dead, still
runs 3.50x hand-rolled.

So RE2's ceiling on this workload is somewhere inside the 36.9ms -> 15.3ms gap
that `ecs-sized` already captures **today, with no refinement types at all**,
and the remaining 15.3ms -> 4.4ms is not something a refined index addresses.
A cheaper first move, if the goal is speed rather than the type-level
property: give the unsized storage a `dense-reserve!` / non-growing
`dense-set-fast!` path, and see how much of the 36.9 -> 15.3 it recovers for
free.

None of that argues against RE2 as a *correctness* feature -- the compile-time
rejection of an out-of-range slot is worth something on its own terms. It
argues against justifying it on this benchmark.

## Files

| file | role |
|---|---|
| `integrate-baseline.c` | hand-rolled C -- the denominator |
| `integrate-manual.tur` | raw Turmeric, flat buffers, no ECS |
| `integrate-ecs.tur` | `defworld` + `for-each2` |
| `integrate-sized.tur` | `sized-defworld` + `sized-for-each` |
| `integrate-poly.tur` | `(HasPos W)`, dictionary lookups in the loop |
| `integrate-poly-hoisted.tur` | same, lookups hoisted -- the ECB control |
| `run.sh` | builds, runs best-of-N, checks checksums, prints the table |

The benchmarks live outside `tests/` on purpose: they are timing programs, not
assertions, and `tur test` recurses.
