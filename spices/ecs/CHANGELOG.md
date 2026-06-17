# tur-ecs changelog

All notable changes to the `tur-ecs` spice are documented here.

## [Unreleased]

### Added

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
