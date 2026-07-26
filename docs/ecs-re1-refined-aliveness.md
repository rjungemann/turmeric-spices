# RE1 -- Strict aliveness accessors in `tur-ecs` (gap C2)

**Date:** 2026-07-26
**Phase:** RE1 of `ecs-refinement-typed-apis-plan.md`, on top of C2
(`refine-stateful-measures-plan.md`).
**Status:** the pattern is proven end-to-end; two compiler dependencies were
found and resolved along the way; harness integration + promotion to a shipped
accessor module remain.

## What RE1 delivers

An aliveness-refined accessor that will not compile against an entity whose
liveness has not been established in a `frozen` region:

```turmeric
(defn get-x! [^borrow w : GameWorld e : #refine{ x : Entity | (alive? w x) }] : int
  (.posx w))

(frozen w
  (if (alive? w e) (get-x! w e) -1))   ;; discharges from the guard: refine: 1 proven
```

`alive?` reads liveness through an OPAQUE handle (a malloc'd `gens` array, the
same shape as `sized-alive?`'s `gens[idx]` compare), so it is impure to the
purity walk and carries `#reads w`. `despawn!` takes `^unique ^mut w`, so a
`frozen` borrow makes it uncallable in the region (`TUR-E0200`) -- which is what
lets the guard discharge soundly.

Fixtures (verified with `--enable=refined --strict-refine` on a Debug `tur`):

- `spices/ecs/tests/refined/alive-frozen.tur` -- guard + read inside `frozen`:
  **refine: 1 proven**, runs -> `42`.
- `spices/ecs/tests/errors/refined-alive-no-region.tur` -- the same read with NO
  region: **1 unknown -> TUR-W0372**. So `#reads` + `frozen` is load-bearing
  here, not inert.
- `spices/ecs/tests/errors/frozen-despawn-in-region.tur` (pre-existing) -- a
  despawn inside the region is `TUR-E0200`.

## Two compiler dependencies found + resolved (turmeric side, 2026-07-26)

1. **`#reads`-refined params could not codegen** -- the impure entry contract was
   `TUR-E0375`. Fixed by suppressing the (unemittable) entry contract for a
   `#reads`-measure predicate; the accessor's own internal check is the runtime
   backstop.
2. **The `frozen` MACRO did not compose with guard-discharge** -- macro expansion
   copies the body, so the path walk missed the crossing (pointer identity).
   Fixed with a source-span crossing match (`rt_form_ident`). Both are in the
   compiler repo with full validation; see
   `docs/archive/frozen-macro-breaks-refinement-guard-discharge.md`.

## Remaining RE1 work

- **Harness integration -- DONE (2026-07-26).** `tur test` gained per-test
  directives (`;; tur-test-flags: --strict-refine`,
  `;; tur-test-expect-error: TUR-W0372`). The RE1 tests are now flat in `tests/`
  and auto-run under `tur test tests`: `tur test` reports 4 passed -- the two
  positives ENFORCE `1 proven` (strict makes an unproven crossing a hard error),
  the two negatives are checked expect-error tests. Files:
  `tests/refined-alive-frozen.tur`, `tests/refined-module-alive-frozen.tur`,
  `tests/refined-alive-no-region.tur`, `tests/refined-module-no-region.tur`.
- **Promotion to a shipped accessor family.** These fixtures use a self-contained
  `GameWorld` facade. Promoting to a real `ecs` module means a facade that owns a
  `WorldState` + storages and re-exports `alive?`/`despawn!`/`get!` with the
  encapsulation (private state field) the soundness argument needs.
- **`for-each` bodies** carrying the aliveness refinement (RE1 item 2) -- the
  highest-value version, deferred.

## Shipped accessor module: `ecs/refined-world` (2026-07-26)

The `GameWorld` facade above is promoted to a real, reusable module,
`ecs/refined-world` (registered in `build.tur :exports`). It exports `RGWorld`
plus `rgworld-new` / `rgworld-spawn!` / `rgworld-despawn!` (`^unique ^mut`) /
`rgworld-alive?` (`#reads`) / `rgworld-get-x!` (refined) / `rgworld-set-x!`. The
backing liveness/component handles and the raw unwrap are PRIVATE.

Validated cross-module (the accessor + measure live in the module; the importer
guards in ITS OWN `frozen` region):

- `tests/refined/module-alive-frozen.tur` -- **1 proven**, runs -> `42`.
- `tests/errors/refined-module-no-region.tur` -- no region -> **TUR-W0372**.

**Trust boundary (also on the module docstring).** `RGWorld` is an opaque affine
handle, so a `frozen` borrow locks out its `^unique ^mut` mutators (`TUR-E0200`)
for ordinary code. But `::` is a coercing cast: `(:: w :int)` unwraps the backing
handle and `(:: int RGWorld)` reconstructs an alias, so a caller that deliberately
does that -- or uses inline-C -- can despawn inside the region. That is the same
escape hatch inline-C already is and the same trusted boundary `#reads` rests on;
a hard adversarial guarantee would need a language feature (module-private
construction / a `::`-sealed newtype). Filed as
`turmeric/docs/reported/frozen-region-aliasing-via-coercing-cast.md`.
