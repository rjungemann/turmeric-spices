# tur-ecs

Entity-Component-System for Turmeric. Pairs with `tur-raylib` for
real-time games. Long-form plan and rationale:
[../../../turmeric/docs/upcoming/ecs-spice-plan.md](../../../turmeric/docs/upcoming/ecs-spice-plan.md).

## Status

**E0 -- skeleton.** Shipped 2026-06-10.

What's in:

- `ecs/entity` -- generationally-versioned opaque `Entity` (low 32 = index,
  high 32 = generation), construct/pack/unpack helpers.
- `ecs/storage` -- dense storage: `(dense-new) -> int`, `dense-set!`,
  `dense-get`, `dense-has?`, `dense-len`, `dense-free`. Single
  int-carried element type per storage; multi-field components require a
  heap-handle wrapper for now (see Known limitations below).
- `ecs/world` -- `defworld Name [Comp1 ... CompN]` macro (arity-capped at
  4 in E0; see Plan E1 for the variadic lift). Lowers to a `defstruct`
  whose fields are named after each component, so component access is
  `(.Pos w)`. `defcomponent` is a documentation-only marker in E0.
  `world-alloc-entity!` and `world-despawn!` manage the generation vec.

What's not in (yet):

- Queries (`for-each`, `(query [...])`, filters) -- ships in E1' /E1.
- Sparse and tag storages -- ship in E1'.
- Systems and the parallel scheduler -- ship in E2.
- raylib companion + demo -- ships in E3.

## Smoke test

```sh
tur run tests/spawn1k.tur
# expected output: 499500
```

Spawns 1000 entities, writes a `Health` value at each slot, then iterates
the dense storage and sums. The script's exit status is 0 on success.

## Known limitations (E0)

Two language-level gaps surfaced during E0 and are filed in the main
repo's `docs/reported/`:

1. **`set-Pos!` / `get-Pos` accessors are not emitted by `defworld`.**
   The macro evaluator does not expose a `str->sym` builtin, so a macro
   cannot mint identifier names like `set-<Comp>!`. The E0 surface is
   the polymorphic `dense-set!` / `dense-get` operating on the typed
   storage field (`(.Pos w)`). See
   [docs/reported/ecs-macro-symbol-synthesis-missing.md](../../../turmeric/docs/reported/ecs-macro-symbol-synthesis-missing.md).

2. **Multi-field components cannot reach `dense-set!` directly.**
   A `(defn [A] ... :A)` with an inline-C body monomorphises `A` to
   `int64_t` in the generated C signature, so passing a `struct Pos`
   by value to `dense-set!` fails at C compile time. Until that lands,
   components are int-carried (raw ints or opaque handles around heap
   allocations). See
   [docs/reported/generic-inline-c-struct-arg-monomorphises-to-int64.md](../../../turmeric/docs/reported/generic-inline-c-struct-arg-monomorphises-to-int64.md).

Both are noted in the plan's "Deferred to v2" / "v2 track" sections and
do not change the v1 roadmap; they shape what the *E0 smoke test* could
exercise, not what eventually ships.

## License

MIT.
