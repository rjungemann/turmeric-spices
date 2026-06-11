# tur-ecs

Entity-Component-System for Turmeric. Pairs with `tur-raylib` for
real-time games. Long-form plan and rationale:
[../../../turmeric/docs/upcoming/ecs-spice-plan.md](../../../turmeric/docs/upcoming/ecs-spice-plan.md).

## Status

**E1' -- fixed-arity queries.** Shipped 2026-06-11.

### What's in

- `ecs/entity` -- generationally-versioned opaque `Entity` (low 32 = index,
  high 32 = generation), construct/pack/unpack helpers.
- `ecs/storage` -- dense storage: `(dense-new) -> int`, `dense-set!`,
  `dense-get` (int-carrier read), `dense-get-w` (struct-carrier read with
  witness), `dense-has?`, `dense-len`, `dense-free`. Per-instantiation
  monomorphization sizes the data array correctly for both int-carried
  opaques and by-value structs.
- `ecs/sparse` -- open-addressed hash storage: `sparse-new/len/set!/
  get/get-w/has?/del!/free`. Linear-probe with backward-shift deletion;
  rehash at load factor 0.75. Use when "few entities carry this
  component" -- ~16 bytes per *populated* slot vs dense's one
  element-sized slot per *world entity*.
- `ecs/tag` -- bitset storage for zero-payload markers (`Dead`,
  `Frozen`, `Player`). `tag-new/cap/count/set!/clear!/has?/free`. O(1)
  set/test, popcount via `__builtin_popcountll`.
- `ecs/world` -- `defworld Name [Comp1 ... CompN]` macro (arity-capped
  at 4 in E0; mixed dense/sparse/tag fields all hold an `:int` handle
  and the same `(.Comp w)` syntax works regardless of backend).
  `world-alloc-entity!`, `world-despawn!`.
- `ecs/query` -- `for-each1`, `for-each2`, `for-each3` imperative
  iteration over dense storages, with the body spliced inline (no
  closure allocation). `world-tagged?` / `world-untagged?` consult a
  tag-bitset field on the world. Compose tag filters with `when` /
  `unless` inside the body to express `with` / `without` constraints.

### What's not in (yet)

- Functional `(query [...])` / `run-query!` (the plan's aztecs-style
  surface). The arity-N macros cover the same iteration shapes; the
  functional packaging lands after the macro-evaluator gaps below
  close.
- Sparse/tag participation in `for-eachN` iteration order. Today the
  primary iteration in `for-eachN` walks the dense-storage union; a
  sparse-primary variant (iterate the smallest sparse storage, look up
  the others) is straightforward but unwritten.
- Systems and the parallel scheduler -- ships in E2.
- raylib companion + demo -- ships in E3.
- Variadic `(query [...])` -- E1 in the plan, gated by variadic HKT
  rows (`docs/reported/variadic-hkt-rows-missing.md`).

## Smoke / regression tests

```sh
tur run tests/spawn1k.tur                 # E0 int-carrier dense (sum = 499500)
tur run tests/spawn1k-pos.tur             # E1' multi-field Pos via dense-get-w (sum = 499500)
tur run tests/sparse-rt.tur               # E1' sparse insert / get / has / del
tur run tests/sparse-rt-large.tur         # E1' patch-1 RH regression (500/250/0)
tur run tests/sparse-stress.tur           # E1' patch-1 10k mixed ops vs bitset (-> ok)
tur run tests/tag-rt.tur                  # E1' tag set / has / clear / popcount
tur run tests/integrate2.tur              # E1' for-each2 Pos+Vel integrate (sum = 125750)
tur run tests/filter-with-without.tur     # E1' tag filters inside body (sum = 1368)
```

Each exits 0 on success.

## Known limitations (E1')

Filed in the main repo's `docs/reported/`:

1. **Per-component named accessors (`set-Pos!`, `get-Pos`) cannot be
   emitted by `defworld`.** The macro evaluator does not expose
   `str->sym`. The current surface is polymorphic
   `dense-set!` / `dense-get` on the typed storage field. See
   [docs/reported/ecs-macro-symbol-synthesis-missing.md](../../../turmeric/docs/reported/ecs-macro-symbol-synthesis-missing.md).

2. **Generic `[A]` returns don't infer from caller context.** Forces
   the `dense-get-w` / `sparse-get-w` witness-passing variant for
   struct components. See
   [docs/reported/generic-return-type-not-inferred-from-context.md](../../../turmeric/docs/reported/generic-return-type-not-inferred-from-context.md).

3. **Backquote silently drops sibling forms when a `~(dot-sym ...)`
   unquote appears in the same macro body.** Forces `(list ...)` form
   construction throughout `ecs/query.tur`. See
   [docs/reported/macro-backquote-dot-sym-drops-siblings.md](../../../turmeric/docs/reported/macro-backquote-dot-sym-drops-siblings.md).

4. ~~Sparse backward-shift deletion leaks ~1% of entries whose probe
   chain wraps the table.~~ **Fixed** by rewriting `ecs/sparse` as a
   full Robin Hood table with a parallel `uint8_t probe_dist[]` array
   (insert does the RH swap, delete consults `probe_dist`, lookup
   short-circuits on the RH monotonicity invariant). Regression covered
   by `tests/sparse-rt-large.tur` (500-entry repro) and
   `tests/sparse-stress.tur` (10,000 mixed ops cross-checked against a
   ground-truth bitset). See
   [docs/reported/ecs-sparse-backward-shift-loses-wrapping-entries.md](../../../turmeric/docs/reported/ecs-sparse-backward-shift-loses-wrapping-entries.md).

5. **`for-eachN` iterates dense storages only.** Listing a sparse or
   tag component on the primary loop would either need a different
   primary (smallest-set iteration) or a dense-then-test fallback.
   Today, use sparse/tag components only via filters inside the body.

None of these change the plan; they shape the *surface ergonomics*
of E1', not what eventually ships.

## License

MIT.
