# tur-ecs changelog

All notable changes to the `tur-ecs` spice are documented here.

## [Unreleased]

### Added

- **Cross-world followups (CAP-V0, CLAUSE-V0, GEN-V0, MULTI-MIR-V0,
  N-W-V0) -- per
  `docs/archive/history/ecs-cross-world-followups-plan.md`.** Five rough edges
  in the shipped cross-world surface are smoothed:
  - **MULTI-MIR-V0 -- multi-component `defmirror`.** `defmirror`
    accepts a `:components [[SrcC DstC] ...]` vector (with
    `:component <entry>` as one-element sugar) and lowers to one
    `defxsystem` whose `:reads-from src [...]` and `:writes-to dst
    [...]` clauses carry every named component. The scheduler sees a
    single system holding the full read+write cap bundle; users who
    want independent per-component scheduling keep separate
    `defmirror`s. Entries are `[Src Dst]` pairs (not bare symbols)
    because `sized-defcomponent-accessors-xmono` mints `set-<C>!` /
    `get-<C>` in one global namespace -- the pair form forces distinct
    names per world. Regression: `tests/xmirror-multi.tur` (3-component
    extract from a `SimWorld` to a `RenderWorld`, round-trip verified).
  - **GEN-V0 -- macro-emitted per-component cid constants.** A new
    `defcomponent-cids` macro in `ecs/world` takes a component vector
    and emits `(def <Comp>-cid N)` for each entry, numbered in
    declaration order. Pair one call per world to retire the
    hand-numbered `(def Pos-cid 0) (def Vel-cid 1) ...` boilerplate that
    every cross-world setup previously had to keep in sync by hand;
    numbering is stable per declaration order, so reordering is a
    breaking change to any external consumer of cid numbers. Per-world
    `box-<W>` / `load-<W>` / `free-<W>-box` were already covered by
    `defworld-box-helpers` in `ecs/xworld` (shipped earlier under the
    same plan tag). Regression: `tests/defcomponent-cids.tur` (two
    independent cid blocks in one TU number from 0).
  - **N-W-V0 -- N-world `defxsystem` + `XStage`.** `defxsystem` now
    accepts *three or more* world bindings, not just two. The clause
    parser generalises to any world count and validates that every
    declared world appears in at least one `:reads-from` / `:writes-to`
    clause (a dropped world fails to compile, naming the world). The
    two-world expansion is left byte-for-byte unchanged; a system over
    `>= 3` worlds lowers to an array-ABI run-fn (`NAME-fn [hp]` reading
    one boxed world handle per slot from an int64 array) plus a new
    `XSystemN` value carrying its per-world masks in heap arrays. The
    scheduler (`ecs/xstage`) stores each system as a *ragged
    `(world-id, reads, writes, handle)` vector* with an ABI tag, so
    conflict detection, wave partitioning, and cycle detection are
    uniform over any slot count; the original two-world API
    (`make-xsystem` / `bind-xsystem` / `make-bound-xsystem` /
    `xstage-add!`) is preserved exactly. New surface: `XSystemN`,
    `make-xsystem-n`, `xsystemn-*` accessors, the int64-array helpers
    `xsm-alloc` / `xsm-set!` / `xsm-get` / `xsm-free` (`ecs/xsystem`);
    `BoundXSystemN`, `make-bound-xsystem-n`, the variadic `bind-xsystem-n`,
    and `xstage-add-n!` (`ecs/xstage`). Regression:
    `tests/xworld-3world.tur` (a 3-world snapshot -> predicted -> render
    extract that schedules to one wave and serialises two conflicting
    bindings into two) and `tests/errors/xworld-unused-world.tur`
    (declared-but-unused world rejected).
  - **CAP-V0 -- the 64-system-per-wave hard cap is gone.** `ecs/xstage`
    sizes its per-wave index / thread / arg buffers to the actual wave
    occupancy (heap, wave-scoped) instead of a fixed `[64]`; a wave with
    more than 64 ready systems no longer silently truncates. Regression:
    `tests/xstage-wide-wave.tur` (200 systems in one wave all run).
  - **CLAUSE-V0 -- flexible clause order.** `defxsystem` accepts its
    `:reads-from` / `:writes-to` clauses in any order, and a world that
    only reads (or only writes) may omit the empty side rather than
    spell out `:writes-to W []`. Regression:
    `tests/xsystem-clause-flex.tur`.

- **Cross-world systems (X1-X4) -- one `defsystem` body spanning two
  typed worlds.** A new surface lets a single system read from one world
  and write to another, with the scheduler proving non-conflict per
  *(world, component)* pair:
  - `ecs/xcap` -- per-`(World, Component)` capabilities `XWriteCap<W, T>`
    / `XReadCap<W, T>` (two-param `:linear` / non-linear opaques). The
    world is part of the type, so `XWriteCap<RenderWorld, Pos>` and
    `XWriteCap<SimWorld, Pos>` are nominally distinct lock targets; a
    `set-<Comp>!` minted for the wrong world rejects the cap (TUR-E0001).
  - `ecs/xworld` -- `sized-defcomponent-accessors-xmono`, the world-keyed
    `get`/`set!`/`has?` trio consuming the two-param caps.
  - `ecs/xsystem` -- the `defxsystem` macro for the repeatable
    `:reads-from W [...]` / `:writes-to W [...]` clauses, lowering to a
    two-world-handle impl with per-`(world, comp)` cap bindings
    (`<w>-<Comp>-{read,write}-cap`) auto-consumed at body end.
  - `ecs/xstage` -- a two-worlds-per-system scheduler keyed on
    `(world-id, component)`: distinct worlds never conflict (coalesce
    into one parallel wave) while same-`(world, component)` writes
    serialise. `xstage-has-cycle?` statically rejects a stage with a
    mutual cross-world ordering dependency.
  - `ecs/xmirror` -- `defmirror`, one-line sugar for the verbatim
    one-component copy between two worlds, lowering to a `defxsystem`.
  This builds on `ecs/hstage` (one-world-per-system cross-world
  scheduling). Regressions: `tests/xworld-extract.tur`,
  `tests/xworld-scheduling.tur`, `tests/xworld-cycle.tur`,
  `tests/xworld-mirror.tur`, plus `tests/errors/xworld-wrong-world-cap.tur`
  and `tests/errors/xworld-undeclared-write.tur`. Guide:
  `docs/guides/ecs-cross-world-guide.md`. Two surface deviations from the
  plan are forced by current `tur` name resolution and documented in the
  guide: caps are named `XWriteCap`/`XReadCap` (a two-param `WriteCap`
  would collide with `ecs/cap`'s one-param `WriteCap` in `tur`'s single
  global opaque namespace), and world bindings take a bare type symbol
  (`[sim SimWorld ...]`) rather than `:SimWorld`.

- **Sized-scheduler direction 2 -- cross-world / heterogeneous
  scheduling (`ecs/hstage`).** A new `HStage` runs systems over
  *differently-typed* worlds in one stage, each system dispatched against
  its own boxed world rather than the single shared world `ecs/stage`
  passes. `bind-system` lifts any `System` (e.g. one produced by
  `sized-defsystem-scheduled`) into a `WorldSystem` keyed on a
  `(world-id, world)` pair, so the cap-gated body / mask machinery is
  reused verbatim. The conflict test is lifted to a (World, Component)
  key: two systems over *distinct* worlds never conflict even when their
  component masks overlap, so the scheduler proves cross-world
  non-conflict statically and coalesces them into one parallel wave;
  same-world overlapping writes still serialise. This was the last Track B
  follow-up, gated on gap-H world-type polymorphism -- the
  typeclass-bounded-wrapper heterogeneous monomorphisation that closed
  upstream in turmeric PRs #447 (multi-param head) and #448 (single-param
  + associated-type head, the spice's `StorageOps` shape). Regression:
  `tests/hstage-cross-world.tur` (two sized world types, cross-world
  parallel wave + same-world serialisation).

### Changed

- **E2d-P6 follow-up -- `defcomponent-accessors` now routes through
  `StorageOps`.** The generated `get-<Comp>` / `set-<Comp>!` /
  `has-<Comp>?` accessors dispatch the `StorageOps` methods
  (`storage-get` / `storage-insert!` / `storage-has?`) against the
  component's `(Storage Comp)` field instead of the hard-coded `dense-*`
  family. The accessor body is now backend-agnostic: a component whose
  `Component` instance binds `(Sparse Comp)` drives the sparse instance
  through byte-identical accessor code -- the "swap the backend with one
  line" payoff, now carried by the type system rather than prose.
  Struct-by-value components round-trip cleanly through `storage-get`'s
  associated `Elem` return; this was the last gate, unblocked by
  turmeric's parametric struct-element projection landing (turmeric
  PR #446, 2026-06-19). `ecs/world` imports `ecs/storage-ops` so the
  routed methods reach the call site transitively through
  `(import ecs/world)` -- no new import at the call site. Regressions:
  `tests/defcomponent-accessors.tur` (dense, struct round-trip) and the
  new `tests/defcomponent-accessors-sparse.tur` (same accessors over a
  sparse-backed component).

### Added

- **E2c slice 11 follow-up -- `sized-defworld-world-resize`, the
  `world-resize` existential wrapper around `sized-defworld-copy-into`.**
  Emits a per-world `world-resize-<Name>` that grows a sized world to a
  fresh *runtime* capacity and hands the result back inside the
  size-hiding existential `(exists [n'] (<Name> n'))`. It is the thin
  client layer the sized-world plan calls out: it allocates the larger
  destination via `make-<Name>`, delegates the slot-preserving,
  generation-threading copy to the already-emitted `copy-into-<Name>`
  (so an `Entity` packed against the source stays `sized-alive?` in the
  resized world), and lifts the destination into the existential with a
  native `(pack dst (exists [n'] (<Name> n')))` -- exactly what the
  stdlib `pack-sized` macro expands to, written inline so the spice
  carries no `load`-time dependency on `stdlib/sized-handle-existential`.
  Callers recover an abstract `n'` via native `open` (or stdlib
  `open-sized`) and run `sized-for-each` / the cap-gated accessors
  against the opened world.

  The new capacity is a runtime argument, which is *why* the result is
  existential: the destination cap is not statically known, so it is
  sealed behind a fresh `n'` rather than surfaced as a concrete
  `(<Name> (Static k))`. The destination's phantom cap is written as
  `(Static 0)` only to give `make-<Name>` a ground type to monomorphise
  against (the constructor codegens only at a ground capacity); it is
  forgotten the moment the value is packed and never reaches runtime --
  every storage and the state cell are sized from the runtime argument,
  and `sized-dense-cap` / `sized-cap` read that runtime width, so
  `sized-for-each`'s loop bound and the accessors' bounds checks all see
  the real capacity. Growing is the supported direction; the underlying
  `sized-state-copy-into` aborts before any partial state is observable
  if the new capacity is smaller than the source (matching
  `copy-into-<Name>`'s shrink-rejection guarantee). Unblocked by
  turmeric's existential pack/open heap-boxing fix for multi-field
  struct payloads. New regression test `tests/sized-world-resize.tur`
  grows a despawn-punched cap-4 world into a runtime cap-8 existential
  and asserts Pos round-trip, generational aliveness preservation, live
  count, and a `sized-for-each` sweep over the opened abstract-`n'`
  world. The component vector lives in exactly one place (the
  `sized-defworld-copy-into` call); `sized-defworld-world-resize` takes
  only the world name.

- **E2c sized-scheduler wiring -- `sized-defsystem-scheduled` macro
  for one-call System lowering against the parallel `Stage`.** The
  sized-scheduler report's direction-1 follow-up
  (`docs/archive/sized-scheduler-system-stage-world-carrier.md`)
  ships at the macro layer, superseding the earlier hand-rolled
  recipe: `(sized-defsystem-scheduled name WorldName [reads] [writes]
  body)` expands to three top-level forms -- a typed
  `name-impl : [^borrow w : WorldName] : nil` with the same
  cap-binding / auto-consume body lowering as `sized-defsystem`, an
  int-carrier wrapper `name-fn : [wp : int] : nil` that loads the
  boxed world via `load-<WorldName>` and dispatches to the typed
  impl, and a `name = (make-system READS-MASK WRITES-MASK name-fn)`
  System value runnable on the parallel `Stage` scheduler. Cap-binding
  guarantees come back compared to the prior hand-rolled `[wp : int]`
  run-fns that minted caps inline and bypassed `defsystem--with-
  write-caps` -- a body that writes a component not declared in
  `:writes` fails to elaborate with `unbound symbol '<Comp>-write-cap'`.

  The per-world `box-<W>` / `load-<W>` / `free-<W>-box` triple stays
  user-written: macros cannot splice identifiers into inline-C text
  (`docs/archive/history/macro-cannot-emit-inline-c-block.md`) and
  the three helpers reference the C struct name verbatim. They are
  three short blocks per world; the macro takes them as a naming
  convention (`load-<WorldName>` is invoked from the wrapper's
  `let`).

  `tests/sized-stage.tur` is refactored onto the new macro: the
  two-system disjoint-mask wave still runs in a single wave (the
  scheduler's conflict-graph non-conflict proof is unaffected by the
  carrier wiring), and the previously-bypassed cap-binding now
  guards both `physics` and `combat`. Generalising `System` /
  `Stage` over the world type so the run-fn carries `^borrow w :
  (WorldName n)` directly (the report's direction 2 / gap-H) is
  still gated on the monomorphisation-audit M2-M7 path; the
  monomorphic-world heap-box landing is the supported surface in
  the interim.

- **E2c slice 12 -- fallible `sized-spawn` returning `(Result int
  WorldFull)`.** The sized-world plan's Q3 result-returning spawn, the
  typed counterpart of the panicking `sized-spawn!`. On success it
  returns `(ok entity)` (the same packed `int` handle `sized-spawn!`
  hands back); on capacity exhaustion it returns `(err world-full)`
  instead of aborting, where `WorldFull` is a new payload-less marker
  type exported from `ecs/sized-world`. The slot-allocation path is
  shared verbatim with `sized-spawn!` (free-list pop, else `next`
  high-water advance, generation read off the slot), so a world that
  despawns to make room succeeds on the next `sized-spawn` and the
  returned entity is generation-correct. Q3 wanted both variants:
  `sized-spawn` where capacity exhaustion is recoverable, `sized-spawn!`
  on benchmark / demo paths where the Result unwrap is pure overhead.

  This was previously deferred behind the M3 struct-carrier path; Track
  A's audit refresh (turmeric `docs/parallel-tracks.md`, "Not a
  blocker") establishes that a spice surface returning `(Result T E)`
  over primitive int-carried payloads -- which both the ok-arm entity
  `int` and the `WorldFull` marker are -- monomorphizes by-value and is
  not M3-gated. One spice-side wrinkle remains: the carrier->Result
  bridge fires at a direct tail-return of `(ok ...)` / `(err ...)` but
  not when the constructor call sits in an `if` branch (the branch
  result is materialized into a struct temp before the return, skipping
  the bridge). `sized-spawn` routes each arm through a one-line helper
  (`__sized-spawn-ok` / `__sized-spawn-err`) whose body is a bare tail
  `(ok ...)` / `(err ...)`, so the bridge fires inside the helper and
  the `if` dispatches between two struct-returning calls -- the
  spice-side idiom for this carrier-bridge tail-position limitation
  until the if-branched form bridges directly. New regression test
  `tests/sized-world-spawn-result.tur` exercises a cap-2 world: two
  successful spawns, an `err` on the full world, then a successful
  re-spawn after a despawn frees a slot.

- **E2c slice 11 -- `sized-defworld-copy-into` for slot-preserving
  world resize.** The sized-world plan's "grow the world" surface:
  open the existential, allocate a fresh `(World n')` with `n' >=
  n`, copy components, close. This slice ships the copy step --
  per-world via `sized-defworld-copy-into`, which emits a
  `copy-into-<Name>` function polymorphic in both source and
  destination capacity:

      (sized-defworld GameWorld [Pos Vel])
      (sized-defworld-copy-into GameWorld [Pos Vel])
      ;; => (defn copy-into-GameWorld [n n']
      ;;       [^borrow src : (GameWorld n)
      ;;        ^borrow dst : (GameWorld n')] : nil ...)

  The body walks `[0, src.cap)`, copies each populated dense slot
  into `dst`'s same-indexed slot, then threads the full state cell
  (gens array, live count, next high-water mark) from src to dst
  via the new `sized-state-copy-into`. The gens copy is what makes
  the resize **Entity-handle preserving**: an `Entity` packed
  against `src` continues to satisfy `(sized-alive? dst e)` after
  the copy, and a despawned entity stays dead in dst because the
  bumped gen flows through. Growing resizes (n' > n) work
  directly; shrinking (n' < n) aborts in `sized-state-copy-into`
  before any partial state is observable. `(GameWorld n)` and
  `(GameWorld n')` are kept type-distinct -- the SZ8 unifier does
  not collapse the two caps, which is what makes the resize
  signature even expressible.

  The component vector must be repeated (the storage handles are
  defopaque ints at the C ABI, so the macro can't recover the
  component list from the world type alone). Out of scope for this
  slice: the plan's `world-resize` wrapper that lifts a copy into
  the `pack-sized` / `open-sized` existential; that is a thin
  client layer over `copy-into-<W>` and adds the
  `(exists [n'] ...)` packaging the plan calls out. New regression
  test `tests/sized-world-copy-into.tur` exercises a cap-4 -> cap-8
  grow with a despawned-mid-population entity, asserting
  Pos round-trip, generational liveness on three surviving
  entities, generational mismatch on the despawned one, and a
  preserved live count.

- **E2c slice 10 -- monomorphic `sized-defworld-mono` +
  `sized-defcomponent-accessors-mono`.** The sized-world plan's
  "ergonomic-default for application code with a fixed budget"
  surface: capacity baked in at declaration, no `[n]` ascription
  required at call sites. `(sized-defworld-mono GameWorld (Static
  64) [Pos Vel])` lowers to a non-parameterised `defstruct` whose
  fields are `(SizedDense (Static 64) Comp)` plus a `make-GameWorld`
  constructor; `(sized-defcomponent-accessors-mono GameWorld Pos)`
  emits the cap-gated `get-Pos` / `set-Pos!` / `has-Pos?` family
  with `w : GameWorld` (no type-arg vector). The polymorphic
  `sized-defworld` / `sized-defcomponent-accessors` remain for
  libraries that ship reusable world shapes.

  The monomorphic body uses new-style `[field : type]` field
  syntax, which (unlike the old-style `(field type)` groups the
  polymorphic `sized-defworld` is forced into by its `[n]` type-
  param vector) accepts the fully-applied `(SizedDense (Static k)
  Comp)` slot directly -- so the original "defstruct field-type
  slot does not accept an unquote-spliced list" rationale from
  `sized-defworld`'s docstring for deferring the monomorphic form
  no longer applies. New regression test
  `tests/sized-defworld-mono.tur` exercises both the macro lowering
  and the mono accessor cap surface.

- **E2c slice 9 -- mixed-shape `sized-for-each`: sparse component
  lookup.** Two new macros in `ecs/sized-query`,
  `sized-world-sparse-has?` and `sized-world-sparse-get`, complete
  the sized-side filter surface by mirroring the slice-6 tag pair
  against `SizedSparse` storages. Use case: walk a dense backbone
  via `sized-for-each` and branch on whether the entity also has a
  sparser component (`Hp`, `Score`, ...); read the sparse value via
  the get macro once presence is confirmed. The element type is
  inferred from the world's typed `.<Comp>` field, so a hand-rolled
  `(GameWorld n)` with `(Hp (SizedSparse n Hp))` lets
  `(sized-world-sparse-get w Hp e)` return an `Hp` without a witness
  arg. Same hand-rolled-world caveat as slice 6: `sized-defworld`
  emits dense fields only, so mixed-shape worlds spell out their
  `defstruct` by hand. New regression test
  `tests/sized-sparse-lookup.tur` -- a (GameWorld n) with dense Pos
  + sparse Hp + sparse Score, iterated as "Pos AND Hp, optionally
  + Score", asserting a weighted sum of 240.

- **E2c slice 8 -- `sized-defsystem`.** Sized-side counterpart of
  `ecs/system`'s `defsystem`. Emits a single `n`-polymorphic
  `(defn name [n] [^borrow w : (WorldName n)] : nil ...)` with the
  declared read/write caps bound in body scope via the same
  `defsystem--with-read-caps` / `defsystem--with-write-caps` /
  `defsystem--consume-write-caps` helpers the unsized macro uses --
  so cap-binding rules and the auto-consume at body end are
  identical, and the load-bearing "writes to a component not in
  :writes is a compile-time error" guarantee carries over verbatim
  (negative test `tests/errors/sized-defsystem-undeclared-write.tur`
  confirms `Vel-write-cap` is unbound when only `[Pos]` is declared
  in `:writes`). The `n`-polymorphic shape mirrors slice 7's
  accessors: one `physics` defn elaborates against every
  `(GameWorld (Static k))` capacity.

  No `System` value is emitted (yet): `ecs/system`'s `System` struct
  pins the run-fn signature to `[w : int] : nil` so the world rides
  the scheduler's `ptr<void>` cast as a bare int, and a
  `(GameWorld n)` is a struct that does not. Wiring sized worlds
  through the parallel scheduler is queued as a follow-up that
  generalizes `System`/`Stage` over the world type; the cap-gating
  is already complete and callers invoke the typed impl directly
  (`(physics game-world)`) until then. New regression test
  `tests/sized-defsystem.tur` exercises the full set/get cycle
  inside a sized-defsystem body against two differently-sized
  worlds.

- **E2c slice 7 -- `sized-defcomponent-accessors`.** Cap-gated
  `get-<Comp>` / `set-<Comp>!` / `has-<Comp>?` for sized worlds,
  the sized counterpart of `ecs/world`'s `defcomponent-accessors`.
  The accessor's world parameter is `^borrow w : (~world-name n)`,
  so `n` is polymorphic at the accessor's signature and unified at
  the call site -- one accessor family elaborates against every
  `(GameWorld (Static k))` shape without duplication (the regression
  test reuses one `set-Pos!` / `get-Pos` / `has-Pos?` family against
  both a `(GameWorld (Static 16))` and a `(GameWorld (Static 4))`).
  Cap surface is unchanged from the unsized accessors: caps pair
  with a component type, not a world shape, so `WriteCap<Pos>` /
  `ReadCap<Pos>` work identically. Bounds checks stay runtime per
  the sized-world plan Q4 -- the static-index `(Fin n)` path is
  refinement-types-gated and out of scope. New regression test
  `tests/sized-defcomponent-accessors.tur`.

- **E2c slice 6 -- `sized-world-tagged?` / `sized-world-untagged?`
  filter macros for sized worlds.** Sized-side analogues of
  `ecs/query`'s `world-tagged?` / `world-untagged?` -- macro-only
  surface that expands to a direct `sized-tag-has?` against the
  named `.<Tag>` field on the world. Composes with `sized-for-each`
  inside the body via `when`/`unless`, exactly the unsized pair's
  ergonomic shape. The world's `.<Tag>` field must hold a
  `(SizedTag n)`; because `sized-defworld` currently emits every
  component field as `(SizedDense n Comp)`, callers mixing tag
  bitsets into a sized world hand-roll the world's `defstruct`
  -- the same constraint the unsized `defworld` has today (see
  `tests/filter-with-without.tur`). New regression test
  `tests/sized-filter-with-without.tur` is the sized counterpart:
  a hand-rolled `(GameWorld n)` with a SizedDense Pos plus two
  SizedTag bitsets (Player / Dead), iterated as "Pos AND Player AND
  NOT Dead". The sum matches the unsized fixture's expected total
  (1368), confirming the macro pair composes with `sized-for-each`
  the same way the unsized pair composes with `for-each`.

- **E2c slice 5 -- `sized-for-each` payoff macro.** New module
  `ecs/sized-query` exporting `sized-for-each`, the bounded-capacity
  counterpart of `ecs/query`'s `for-each`. Shape mirrors the unsized
  macro one-for-one (`(sized-for-each w [Comp...] [e v...] body)`),
  but every storage access goes through the typed `(SizedDense n
  Comp)` surface (`sized-dense-has?` / `sized-dense-get`) and the
  loop bound is the first storage's static `sized-dense-cap` --
  not a per-storage runtime min. Rectangularity is structural:
  `(sized-defworld GameWorld [Pos Vel])` lowers every field to
  `(SizedDense n Comp)`, so every `(.Comp w)` access on a
  `w : (GameWorld n)` carries the same type-level `n` and the SZ8
  cross-parameter unifier proves the storages line up at the world's
  defstruct -- the runtime `__fe-min-cap` probe the unsized `for-each`
  computes is gone. Per-component `sized-dense-has?` checks stay
  because slot population is data, not type. New regression test
  `tests/sized-for-each.tur` exercises an 8-slot world with four
  populated slots in mixed intersection/single-component
  configurations; only the two slots that have both components are
  visited. The `ecs-sized-world-plan.md`'s "for-each -- the payoff"
  surface is now wired.

  Also fills in `ecs/sized-world`'s missing build.tur exports
  (`sized-spawn!`, `sized-despawn`, `sized-alive?`, etc.) so the
  spawn / despawn / alive surface from slices 4 / 4b / 4c is
  reachable from consumers via the normal `:exports` path instead of
  only via module-internal references.

- **E2c slice 4c -- generational `Entity` handles on sized worlds.**
  The slice-4b free-list lets `sized-spawn!` recycle the slot a
  despawn just freed; without per-slot generations a stale handle
  that named the slot before the despawn was indistinguishable from
  the freshly-spawned handle at the same slot -- a use-after-despawn
  hazard the unsized `ecs/world` has guarded against since E0 via
  generational `Entity` packing. Slice 4c brings the sized world up
  to the same surface: the `__ecs_state` control block now carries a
  `int64_t *gens` array sized to `cap`; `sized-spawn!` returns a
  packed `Entity` via `entity-new(slot, gens[slot])`; `sized-despawn`
  takes an `Entity`, verifies its generation against the slot's
  current `gens[slot]`, bumps the slot's generation, and pushes the
  slot onto the free-list. New helpers `sized-alive?` (true iff the
  Entity's (idx, gen) still matches `gens[idx]`) and
  `sized-slot-generation` round out the surface. The generation
  check also makes `sized-despawn` double-despawn-safe -- a second
  call against the stale handle no-ops instead of corrupting the
  free-list with a duplicate slot. Memory stays O(cap) -- the
  `gens` array is `calloc`'d at construction and released alongside
  the state cell. New regression test
  `tests/sized-world-generation.tur` exercises the alive / dead /
  respawn / double-despawn matrix end-to-end.

- **E2c slice 4b -- free-list-based slot reuse in sized worlds.** The
  slice-4 `sized-despawn` decremented `live` but left the freed slot
  stranded -- `next` advanced monotonically, so a world that repeatedly
  spawned and despawned still exhausted its capacity. Slice 4b grows the
  `__ecs_state` control block with a `int64_t *fl_data` free-list (plus
  `fl_len` / `fl_cap`): `sized-despawn` now pushes the freed slot onto
  the list, and `sized-spawn!` pops from it before reaching for a fresh
  slot. A cap-`n` world that recycles slots can run indefinitely within
  its capacity. The free-list is freed alongside the state cell, so
  total memory stays O(cap). New regression test
  `tests/sized-world-reuse.tur` exercises spawn-after-despawn slot
  reuse and a tight despawn/spawn loop that runs 100 cycles against a
  cap-3 world.

### Changed -- BREAKING

- **`sized-spawn!` returns `Entity`; `sized-despawn` takes `Entity`.**
  Slice 4b's intermediate signature handed back / consumed a bare
  slot `int`. Slice 4c lifts both to the packed generational `Entity`
  that the unsized `ecs/world` has used since E0. For a fresh world
  every slot starts at generation 0, so callers that only inspect
  `(entity-index ...)` see the same slot ids as before; the
  generation-bearing high bits only diverge once a slot is despawned
  and reused. Callers update by:
  ```turmeric
  ;; before:
  (let [s (sized-spawn! state)]
    (sized-despawn state s))
  ;; after:
  (let [e (sized-spawn! state)]
    (sized-despawn state e))    ;; no change at call sites that
                                ;; never named the bare slot id
  ;; callers that want the slot id explicitly:
  (entity-index e)
  ```

- **E2d-P6 (stretch) -- polymorphic storage ops via a single-param class.**
  New module `ecs/storage-ops` defines `(defclass StorageOps [S] (type Elem
  : Type) (storage-insert! ...) (storage-get ...) (storage-has? ...))` -- a
  single-parameter class threading the carried element type through an
  associated `Elem` (not a second class parameter, which the unshipped
  multi-param machinery would need). Instances are provided for the `(Dense
  A)` and `(Sparse A)` backends, forwarding to the matching `dense-*` /
  `sparse-*` ops; a body written against the class drives insert/get/has?
  by the storage handle's type, lifting the "swap the backend with one
  line" claim into the type system. `Tag` is deliberately *not* an instance
  (it is payload-less: no element to `get`, no value to `insert!`). See
  `tests/storage-ops-poly.tur`, which reuses the same call sequence over a
  `(Dense Hp)` and a `(Sparse Hp)`. Scope per the plan: every handle *and
  element* rides the int64 carrier, so a by-value struct element is out of
  scope (it monomorphises `Elem` to the int64 carrier and mismatches the C
  ABI) -- struct components keep the direct `dense-*` accessor path.
  `storage-get` needs a use-site witness ascription (`(:: ... Hp)`) because
  the associated `Elem` is not yet projected in return position, mirroring
  the pre-E2d-P1 `dense-get` requirement. The `defcomponent-accessors`
  rewrite-through-`StorageOps` is **deferred**: those accessors carry
  struct components (out of the class's int-carrier scope) and a large
  cap-gated passing-test surface, so routing them through the class would
  regress the suite for no behavioural gain today -- noted as a follow-up
  once struct-element projection lands. Bounded-polymorphic wrappers
  (`[S] [(StorageOps S)]`) also remain off the shipped surface: they fail to
  link against the monomorphised instance symbol, the same gap-H limitation
  as `defcomponent-class`'s typeclass-bounded wrappers; monomorphic
  dispatch works end to end.

- **E2c slice 3b -- variadic `sized-defworld` (uncapped arity).** The
  slice-3 `sized-defworld` unrolled its body per-arity and capped sized
  worlds at four components. That cascade is now collapsed into one
  variadic macro mirroring E2d-P5b: a recursive `sized-world-fields`
  helper builds the `(Comp (SizedDense n Comp))` field groups and a
  `sized-ctor-args` helper builds the per-component constructor arguments,
  both spliced at macro-expansion time. A bounded-capacity world may now
  carry any number of components (see `tests/sized-world-wide.tur`, a
  five-component world). Relies on the same two turmeric fixes E2d-P5b
  used (`~@`-splice into a literal and a nested user-macro call from inside
  a splice expression), which landed 2026-06-17.
- **E2d-P5b -- variadic `defworld` (uncapped arity).** The former
  `defworld--0..5` per-arity cascade is collapsed into one variadic body:
  a recursive `world-fields` helper macro computes the flat
  `Comp : (Storage Comp)` field list at macro-expansion time and splices
  it into the `defstruct` body. A world may now carry any number of
  components (see `tests/spawn1k-wide.tur`, an eight-component world). The
  collapse depends on two turmeric fixes that landed 2026-06-17:
  `~@`-splice into a vector literal and a nested user-macro call from
  inside a `~@` splice expression.
- **E2d -- associated-type storage projection.** A new `Component`
  typeclass in `ecs/world` carries an associated type member
  `(type Storage : Type)`. `(defcomponent T)` now lowers to a
  `(definstance Component [T] (type Storage = (Dense T)))` registration,
  and `defworld` projects every component field type through
  `(Storage T)` (reducing to `(Dense T)`) instead of the old hard-coded
  `: int`. Storage backend choice is now a property of the component,
  visible on the world's type. (E2d-P1..P5; the critical path
  `P1 -> P3 -> P2 -> P4 -> P5` from `ecs-spice-plan.md`.)
- **`ecs/cap`** -- new module exposing `WriteCap<T>` / `ReadCap<T>`
  capability tokens plus `make-write-cap` / `make-read-cap` / `use-cap!`.
  Backs the cap-gated accessor surface in `ecs/world` and the cap
  bindings emitted by `defsystem`.
- **`defcomponent-class-instance`** now also emits per-(World, Comp)
  cap-mint helpers `mint-<World>-<Comp>-write-cap` / `mint-<World>-<Comp>-read-cap`.

### Changed -- BREAKING

- **Storage handles are now `defopaque`, not bare `:int`.** `ecs/storage`
  exports `(Dense A)`, `ecs/sparse` exports `(Sparse A)`, and `ecs/tag`
  exports `Tag`. `dense-new` / `sparse-new` return the typed handle and
  the whole `dense-*` / `sparse-*` / `tag-*` surface takes it, so a
  storage for one component can no longer be passed where another's is
  expected. The carrier is still `:int` (handles erase at codegen), so
  crossing the boundary -- e.g. an int-carried marker component value, or
  a hand-rolled FFI handle -- is a free one-token `::` ascription:
  `(:: i :Health)` / `(:: (dense-new) (Storage Pos))`. Components used in
  a `defworld` must now be real types (a `defstruct`, or a
  `(defopaque Marker :int)` for a payload-less marker) with a
  `(defcomponent ...)` registration. Mixed dense+tag worlds are
  hand-rolled (`defworld` projects every field as dense). Bare-`int`
  "marker" components that skipped a type declaration no longer compile.
- **`defsystem` `:reads` / `:writes` are now component-name vectors.**
  The prior `(defsystem name reads-mask writes-mask body)` bitmask form
  is removed; the new form is `(defsystem name [Comp ...] [Comp ...] body)`.
  Each declared comp must have a `<Comp>-cid` binding in scope (the
  prior `pos-cid` lowercase convention migrates to `Pos-cid`).
  `defsystem` binds `<Comp>-read-cap` / `<Comp>-write-cap` in body scope
  for each declared comp; the `:linear` write-caps auto-consume at body
  end. See the migration note in `README.md` for the before/after
  shape.
- **`defcomponent-accessors`'s `set-<Comp>!` / `get-<Comp>`** now
  require a `(WriteCap Comp)` / `(ReadCap Comp)` as the first
  argument. A body that did not declare `:writes [Vel]` has no
  `Vel-write-cap` in scope, so the `set-Vel!` call fails to elaborate
  -- the load-bearing "writes to a component you didn't list is a
  compile-time error" promise from the original ECS plan.

### Migration

```turmeric
;; Old (pre-Unreleased):
(import ecs/system :refer [defsystem cid-bit])
(def pos-cid 0)

(defsystem physics
  (cid-bit pos-cid)               ;; reads-mask
  (cid-bit pos-cid)               ;; writes-mask
  (dense-set! (.Pos w) e v))      ;; raw setter, no cap check

;; New (Unreleased):
(import ecs/system :refer [defsystem])
(import ecs/cap    :refer [WriteCap ReadCap make-write-cap make-read-cap use-cap!])
(defstruct Pos [x : int])
(def Pos-cid 0)

(defsystem physics
  [Pos]                                 ;; :reads
  [Pos]                                 ;; :writes -- binds Pos-write-cap
  (set-Pos! Pos-write-cap w e v))       ;; cap-gated setter
```

See [`docs/archive/history/ecs-defsystem-write-caps-not-enforced.md`](../../../turmeric/docs/archive/history/ecs-defsystem-write-caps-not-enforced.md)
in the turmeric repo for the original report and the full I1-I6
implementation log.
