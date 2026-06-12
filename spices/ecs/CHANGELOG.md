# tur-ecs changelog

All notable changes to the `tur-ecs` spice are documented here.

## [Unreleased]

### Added

- **`ecs/cap`** -- new module exposing `WriteCap<T>` / `ReadCap<T>`
  capability tokens plus `make-write-cap` / `make-read-cap` / `use-cap!`.
  Backs the cap-gated accessor surface in `ecs/world` and the cap
  bindings emitted by `defsystem`.
- **`defcomponent-class-instance`** now also emits per-(World, Comp)
  cap-mint helpers `mint-<World>-<Comp>-write-cap` / `mint-<World>-<Comp>-read-cap`.

### Changed -- BREAKING

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
