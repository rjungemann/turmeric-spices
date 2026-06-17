# tur-ecs changelog

All notable changes to the `tur-ecs` spice are documented here.

## [Unreleased]

### Added

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
