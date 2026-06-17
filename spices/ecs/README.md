# tur-ecs

Entity-Component-System for Turmeric. Pairs with `tur-raylib` for
real-time games. Long-form plan and rationale:
[../../../turmeric/docs/upcoming/ecs-spice-plan.md](../../../turmeric/docs/upcoming/ecs-spice-plan.md).

## Status

**E1 -- variadic-looking queries + row-typed `Query` value.** Shipped 2026-06-11.

The D1 prerequisite (variadic HKT rows) and all four L6 follow-ups
(strict row elements, `^&` on defgadt/ADT/deftype, row-polymorphic
defn/fn, permutation-aware row equality) landed in the main repo this
session, which unblocked the E1 surface. See
`../../../turmeric/docs/archive/history/variadic-hkt-rows-missing.md`.

### What's in

- `ecs/entity` -- generationally-versioned opaque `Entity` (low 32 = index,
  high 32 = generation), construct/pack/unpack helpers.
- `ecs/storage` -- dense storage: `(dense-new) -> int`, `dense-set!`,
  `dense-get`, `dense-has?`, `dense-len`, `dense-free`. Per-instantiation
  monomorphization sizes the data array correctly for both int-carried
  opaques and by-value structs; annotate the let binding
  (`(let [p : Pos (dense-get s i)] ...)`) to drive the generic into a
  struct-specialised clone.
- `ecs/sparse` -- open-addressed Robin Hood hash storage:
  `sparse-new/len/set!/get/has?/del!/free`. Backward-shift deletion
  driven by `probe_dist`; rehash at load factor 0.75. Use when "few
  entities carry this component" -- ~16 bytes per *populated* slot vs
  dense's one element-sized slot per *world entity*.
- `ecs/tag` -- bitset storage for zero-payload markers (`Dead`,
  `Frozen`, `Player`). `tag-new/cap/count/set!/clear!/has?/free`. O(1)
  set/test, popcount via `__builtin_popcountll`.
- `ecs/world` -- `defworld Name [Comp1 ... CompN]` macro (variadic since
  E2d-P5b -- no arity cap; a recursive `world-fields` helper computes the
  flat field list at expansion time. Mixed dense/sparse/tag fields all
  hold an `:int` handle and the same `(.Comp w)` syntax works regardless
  of backend). `world-alloc-entity!`, `world-despawn!`.
- `ecs/query` -- single user-facing **`for-each`** macro, truly
  variadic (no arity cap; recursive helper macros walk the component
  list at expansion time):
  ```
  (for-each w [Pos Vel]      [e p v]      body)   ;; 2 components
  (for-each w [Pos Vel Hp]   [e p v h]    body)   ;; 3 components
  (for-each w [A B C D]      [e a b c d]  body)   ;; 4 components
  (for-each w [A B ... J K]  [e a ... j k] body)  ;; 11 components, no cap
  ```
  Thin `for-each1`..`for-each3` shims stay exported for back-compat.
  `world-tagged?` / `world-untagged?` consult a tag-bitset
  field on the world. Compose tag filters with `when` / `unless` inside
  the body to express `with` / `without` constraints.
- `ecs/query` ships **`defquery`** + **`run-query!`** -- the functional
  packaging from the plan:
  ```
  (defquery integrate w GameWorld [Pos Vel] [e p v]
    (dense-set! (.Pos w) e (+ p v)))

  (run-query! integrate world)
  ```
  `defquery` desugars to a `(defn integrate [^borrow w : GameWorld] : nil ...)`
  whose body is the `for-each` iteration; `run-query!` is sugar for
  invoking it. The world is taken as `^borrow` so callers can reuse the
  world value after running the query.
- `ecs/query` also exposes the row-typed **`Query`** value:
  ```
  (defstruct Query [^&in ^&out] (world :int))
  ```
  Row arguments are *phantom* -- the variadic-HKT-rows work erases them
  at codegen, so a `Query` carries only the world handle at runtime,
  but two `Query`s with different `(in, out)` row arguments are
  distinguished at the type level. Typical use:
  ```
  ;; A Query against GameWorld that reads Pos+Vel and writes Pos:
  (defn integrate [q : (Query #row{Pos Vel} #row{Pos})] : nil ...)
  ```
  Requires `-Xdata-literals` at the consumer's build for the
  `#row{...}` literal syntax. The spice module itself does not use
  `#row{...}`, so it builds under the default reader.

### What's not in (yet)

- Sparse/tag participation in `for-each` iteration order. Today the
  primary iteration walks the dense-storage union; a sparse-primary
  variant is straightforward but unwritten.
- Systems and the parallel scheduler -- shipped in E2 prior to this.
- raylib companion + demo -- ships in E3.

## Smoke / regression tests

```sh
tur run tests/spawn1k.tur                 # E0 int-carrier dense (sum = 499500)
tur run tests/spawn1k-pos.tur             # E1' multi-field Pos via dense-get (sum = 499500)
tur run tests/sparse-rt.tur               # E1' sparse insert / get / has / del
tur run tests/sparse-rt-large.tur         # E1' patch-1 RH regression (500/250/0)
tur run tests/sparse-stress.tur           # E1' patch-1 10k mixed ops vs bitset (-> ok)
tur run tests/tag-rt.tur                  # E1' tag set / has / clear / popcount
tur run tests/integrate2.tur              # E1' for-each2 Pos+Vel integrate (sum = 125750)
tur run tests/filter-with-without.tur     # E1' tag filters inside body (sum = 1368)
tur run tests/for-each-variadic.tur       # E1  variadic (for-each w [Pos Vel] ...) (125750)
tur run tests/for-each-arity-4.tur        # E1  arity-4 dispatch (49500)
tur run tests/for-each-arity-5.tur        # E1  arity-5 dispatch (18375)
tur run tests/for-each-arity-8.tur        # E1  arity-8 (15660)
tur run tests/for-each-arity-12.tur       # E1  arity-12 no-cap demo (3510)
tur run tests/defquery-integrate.tur      # E1  defquery + run-query! (125750)
tur run -Xdata-literals tests/query-typed.tur   # E1  row-typed Query value (prints 42)
```

Each exits 0 on success.

## Known limitations

Each of the original prerequisite gaps (A-I, archived at
[`docs/archive/history/ecs-prereq-plan.md`](../../../turmeric/docs/archive/history/ecs-prereq-plan.md))
has shipped; the residual list below is empirical limits of the
v1 surface, not language gaps.

1. **`for-eachN` iterates dense storages only.** Listing a sparse or
   tag component on the primary loop would either need a different
   primary (smallest-set iteration) or a dense-then-test fallback.
   Today, use sparse/tag components only via filters inside the body.

### Breaking change in the I3-I4 ship (2026-06-11)

`defsystem`'s `:reads`/`:writes` are component-name vectors, not
bitmask ints. Per-component CIDs use the `<Comp>-cid` titlecase
convention (`Pos-cid`, not `pos-cid`). Required imports at the call
site:

```turmeric
(import ecs/system :refer [defsystem])
(import ecs/cap    :refer [WriteCap ReadCap make-write-cap make-read-cap use-cap!])
```

Then:

```turmeric
;; Before (E2 surface):
(defsystem physics
  (cid-bit pos-cid)
  (cid-bit pos-cid)
  body)

;; After (I3 surface):
(defsystem physics
  [Pos]
  [Pos]
  body)
```

`defcomponent-accessors` now emits `set-<Comp>!` / `get-<Comp>` that
require a `WriteCap<Comp>` / `ReadCap<Comp>` first arg; pass either
the `<Comp>-write-cap` / `<Comp>-read-cap` bound by `defsystem` in
body scope, or a freshly minted cap from `make-write-cap` /
`make-read-cap` (gated by the call site's declared `:writes`). A body
that writes a component it did not declare now fails to elaborate.
See the original report at
[docs/archive/history/ecs-defsystem-write-caps-not-enforced.md](../../../turmeric/docs/archive/history/ecs-defsystem-write-caps-not-enforced.md).

## License

MIT.
