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
  at 5 in E1; mixed dense/sparse/tag fields all hold an `:int` handle
  and the same `(.Comp w)` syntax works regardless of backend).
  `world-alloc-entity!`, `world-despawn!`.
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
tur run tests/spawn1k-pos.tur             # E1' multi-field Pos via dense-get-w (sum = 499500)
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

Filed in the main repo's `docs/reported/` and
[`docs/upcoming/ecs-prereq-plan.md`](../../../turmeric/docs/upcoming/ecs-prereq-plan.md):

1. ~~Per-component named accessors (`set-Pos!`, `get-Pos`) cannot be
   emitted by `defworld`.~~ **Fixed** -- `str->sym` is shipped
   (`src/compiler/elab_macros.c:321-326`); a `defworld` upgrade that
   emits per-component accessors is queued as the next spice change.
   See [docs/reported/ecs-macro-symbol-synthesis-missing.md](../../../turmeric/docs/reported/ecs-macro-symbol-synthesis-missing.md).

2. ~~Generic `[A]` returns don't infer from caller context.~~ **Mostly
   fixed.** Typed bindings (`(let [p : Pos (dense-get s i)] ...)`),
   `::` ascription, and enclosing defn return types all bind A
   correctly now. The remaining case is bare `(let [p (dense-get s
   i)] ...)` followed by struct field access -- A defaults to the
   int carrier there. Use `dense-get-w` / `sparse-get-w` (witness
   variants) or annotate the binding.

3. ~~Backquote silently drops sibling forms when a `~(dot-sym ...)`
   unquote appears in the same macro body.~~ **Fixed** on probe.
   `ecs/query.tur`'s macros are still written in `(list ...)` style
   for the moment; a rewrite-to-backquote sweep is queued as a
   separate change.

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

6. **`defsystem` `:writes` lists are NOT enforced at compile time.**
   The ECS plan calls this "the single biggest delta vs. Haskell
   ECSes" and claims compile-time gating of `set-X!` access on
   `:writes` membership. The shipped `defsystem` collects `:writes`
   as a runtime bitmask used by the scheduler for wave assignment,
   but a system declaring `:writes [Pos]` and calling
   `(dense-set! (.Vel w) ...)` in its body compiles and runs without
   diagnostic. If the user lies in the declaration, the scheduler's
   wave grouping is wrong and runtime races appear. See
   [docs/reported/ecs-defsystem-write-caps-not-enforced.md](../../../turmeric/docs/reported/ecs-defsystem-write-caps-not-enforced.md)
   for triage and three implementation paths (Path A: per-component
   `WriteCap` linear capabilities, recommended).

None of these change the plan; (1)-(3) are upstream fixes that
ship the original plan's API more directly, (5) is a query-engine
follow-up, (6) is the only one that's a real gap against the plan's
spec'd surface (the plan promises it; the spice doesn't deliver it
yet).

7. ~~`query-world` called from a row-polymorphic context fails to link.~~
   **Fixed.** The relay-vs-carrier classifier in
   `src/compiler/emit_module.c` now ignores row-kinded named tyvars
   when deciding whether a call requires specialization; rows are
   phantom so the carrier definition suffices and gets emitted. See
   [docs/reported/row-polymorphic-defn-call-from-row-polymorphic-context-missing-codegen.md](../../../turmeric/docs/reported/row-polymorphic-defn-call-from-row-polymorphic-context-missing-codegen.md).

## License

MIT.
