# Plan: `plot` Renderer Typeclass Collapse (U2, v1)

> Status: planning
> Tracks: spices-type-features-uplift-plan **U2 target — plot**
> Scope: `spices/plot/` only; no compiler dependency expected (see Risks)
> Prereq landed: ECS existential-wrapper pattern (turmeric-spices PR #17)

## Motivation

`plot` is the U2 "typeclass collapse" target whose dispatch is the most
deeply tag-erased in the repo. Every renderer constructor
(`function`, `lines`, `points`, `area`, `contour`, …) calls
`__make-renderer` with a numeric `kind` tag and packs heterogeneous
payload into a fixed bag of generic slots:

```turmeric
;; plot/line.tur
(defn function [f x-min : float x-max : float samples : int style : int label : cstr] : int
  (__renderer-bounds
    (__renderer-ints
      (__renderer-floats (__make-renderer 21 style 0 f 0 0 0 0 label "")
                         x-min x-max 0.0 0.0 0.0 0.0)
      samples 0 0 0)
    x-min x-max (__nan) (__nan)))
```

`__make-renderer` (`plot/core.tur:53`) allocates one C struct with
`kind`, `style1/2`, `data1..data5`, `f1..f6`, `i1..i4`, `label`, `text`,
and four bounds doubles. Each renderer kind interprets these slots
differently, and the meaning is recoverable **only** by reading the
central switch.

That switch lives twice in `plot/core.tur`:

| Site | Lines | Arms |
|---|---|---|
| `RK_*` enum | `core.tur:604-613` | ~32 kinds |
| bounds-union pass (`plot-into-canvas`) | `core.tur:692-771` | per-kind `UNION_*` |
| draw pass (`plot-into-canvas`) | `core.tur:~900-1543` | per-kind drawing |
| adaptive sampler kind switch | `core.tur:367-373, 389-411` | RK_FUNCTION/PARAMETRIC/POLAR/INVERSE |

This is exactly the U2 rubric's "≥3 parallel branches differing only in
payload type", taken to the extreme: one runtime `kind` int drives a
~32-arm `switch`, and the payload slots are `int64_t`-erased. The bugs
this invites are concrete:

- A constructor that stuffs a value into the wrong slot index is a silent
  mis-render, not a type error (`data2` means "category count" for
  `RK_DISCRETE_HIST` but "second point list" for `RK_LINES_INTERVAL`).
- Adding a renderer kind means editing three switches by hand and hoping
  none is missed; a missing arm is a no-op `default:`, not a diagnostic.
- The generic-slot signatures (`__renderer-floats` takes six bare
  `float`s) carry no information about which are meaningful for a kind.

The end state: each renderer kind is its own type with named, typed
fields; a `Renderer` typeclass provides `bounds` and `render` methods;
the central `switch` is replaced by coherent dispatch.

```turmeric
;; target shape
(defstruct LinesR  [points : (Vec Point)  style : LineStyle  label : cstr])
(defstruct FunctionR [f : (-> float float) x-min : float x-max : float
                      samples : int style : LineStyle label : cstr])

(definstance Renderer [LinesR]
  (bounds [r] (union-of-points (.points r)))
  (render [r ctx] (draw-polyline ctx (.points r) (.style r))))
```

---

## Non-goals

- Changing rendered output. Pixels must stay byte-identical; the existing
  tests assert on pixel content (`__count-non-background`,
  `__pixel-is-background?` in `core.tur:249-306`) and those must pass
  unchanged.
- Adding new renderer kinds or backends. There is exactly one backend
  (plutovg); the parallel-tracks note's "per-backend instances" phrasing
  is a misnomer — the dispatch axis is renderer **kind**, not output
  backend. (A second backend, if ever added, becomes a second typeclass;
  out of scope here.)
- Rewriting the intricate C drawing routines into Turmeric. The drawing
  code (adaptive sampling, tick selection, legend layout) stays in C; the
  plan only changes *how it is dispatched* and *how payload is typed*.
- Touching `plot/style.tur`'s `LineStyle`/`PointStyle`/`FillStyle`
  structs beyond importing them as field types.

---

## Strategy

The risk is concentrated in the 800-line C draw pass. The plan keeps that
code intact and attacks dispatch and payload typing around it, in two
independently-shippable halves:

1. **Refactor under the existing API (P0–P1):** split the monolithic
   `plot-into-canvas` switch into one standalone C draw function per kind
   and one bounds function per kind, still selected by the `kind` int. No
   public signature changes; pure internal de-monolithing that makes each
   arm independently callable and testable. This alone is valuable and
   reversible.
2. **Lift to types (P2–P5):** introduce per-kind structs + the `Renderer`
   typeclass whose methods call the P0 C functions, box mixed renderers
   into an existential so `(vec-of (function …) (points …))` still
   type-checks, and migrate constructors behind one-release shims.

If the type lift (half 2) hits a compiler limitation, half 1 still lands
and stands on its own.

---

## Phases

### P0 — De-monolith the draw + bounds switches (no API change)

**Tasks**
- In `plot/core.tur`, extract each `case RK_*:` body from the
  `plot-into-canvas` draw pass into a standalone `defn`
  `__draw-<kind>` taking `(canvas, renderer, viewport, bounds)` and the
  shared style/palette context. Keep them `#{Unsafe}` inline-C; move the
  per-arm code verbatim.
- Extract each `UNION_*` arm (`core.tur:692-771`) into
  `__bounds-<kind> [renderer] : BBox` (a 4-double struct or the existing
  bounds carrier).
- Replace both switches with a single thin dispatcher that still reads
  `r->kind` and calls the extracted function — behavior-preserving.
- Keep `__plot-sample-adaptive` as-is (it is already a standalone helper);
  the sampled-curve `__draw-*` functions call it.

**Acceptance**
- `tur test spices/plot/tests` passes with zero pixel-diff (existing image
  assertions unchanged).
- `plot/core.tur`'s `plot-into-canvas` no longer contains a `switch
  (r->kind)` in the draw or bounds passes — only a dispatch table.

### P1 — `BBox` and a render context value

**Tasks**
- Define `(defstruct BBox [x-min : float x-max : float y-min : float y-max : float])`
  in `plot/core.tur` (replacing the four loose doubles threaded through C).
- Define `(defstruct RenderCtx [...])` capturing what the draw pass
  computes once (viewport rect, resolved bounds, font handle, palette
  index cursor, legend position). The P0 `__draw-*` functions take a
  `RenderCtx` instead of a long positional argument list.

**Acceptance**
- `__bounds-<kind>` returns a `BBox`; the union pass folds `BBox`es.
- `__draw-<kind>` takes `(RenderCtx, renderer)`; no behavior change.

### P2 — Per-kind renderer structs + `Renderer` typeclass

**Tasks**
- For each renderer kind, define a `defstruct` carrying its real fields
  with real types, replacing the generic-slot packing. Group by source
  module: `LinesR`/`FunctionR`/`ParametricR`/`PolarR`/`InverseR`/`DensityR`
  in `plot/line.tur`; `PointsR`/`ErrorBarsR`/`VectorFieldR`/`ArrowsR` in
  `plot/point.tur`; `RectanglesR`/`AreaHistR`/`DiscreteHistR`/`StackedHistR`
  in `plot/area.tur`; the contour/decoration kinds similarly.
- Define the typeclass in `plot/core.tur` (same module as the eventual
  instances, to satisfy the orphan-instance check — mirror the ansi
  `Color` and json `Encode` precedent):

  ```turmeric
  (defclass Renderer [a]
    (bounds [r] : BBox)
    (render [r ctx : RenderCtx] : void))
  ```
- One `definstance Renderer [<KindR>]` per struct, delegating to the P0/P1
  `__bounds-<kind>` / `__draw-<kind>` functions. Each instance now reads
  named fields (`(.points r)`) instead of `data1`.

**Acceptance**
- `tur check` clean on all per-kind instances.
- A unit fixture constructs one struct of each kind and calls `bounds` and
  `render` directly (no Vec yet), producing the same pixels as the legacy
  constructor for that kind.

### P3 — Existential `Renderer` box for heterogeneous vecs

The public API takes a `Vec` of mixed renderer kinds
(`plot-into-canvas` iterates `renderers` as a `Vec[Renderer]`,
`core.tur:678-682`). Coherent typeclass dispatch needs a uniform element
type, so mixed kinds must be boxed.

**Tasks**
- Introduce an existential wrapper `AnyRenderer` that packs a value plus
  its `Renderer` dictionary, following the ECS sized-world existential
  pattern shipped in PR #17 (cite that file as the template).
- Provide `(any-renderer x)` to box any `(Renderer a) => a` and have
  `plot`, `plot-into-canvas`, `render` consume `(Vec AnyRenderer)`.
- The iteration in `plot-into-canvas` calls `bounds`/`render` through the
  boxed dictionary instead of switching on `kind`.

**Acceptance**
- `(vec-of (function …) (points …) (lines …))` type-checks once each
  constructor returns its `*R` struct boxed as `AnyRenderer`.
- Negative fixture: a `Vec` element that is not a `Renderer` instance
  fails to compile (the deliverable type-safety win).

### P4 — Migrate constructors; keep one-release shims

**Tasks**
- Rewrite the public constructors in `plot/line.tur`, `point.tur`,
  `area.tur`, `contour.tur`, `interval.tur`, `decor.tur` to build the
  typed struct and return `AnyRenderer` instead of calling
  `__make-renderer`.
- Keep `__make-renderer` and the generic-slot helpers exported but
  deprecated for one release (some downstream/example code may call them
  directly); mark with a docstring note pointing here.
- Update `spices/plot` README and `docs/guides/plutovg-guide.md` examples
  to the typed constructors (output unchanged, so prose is minimal).

**Acceptance**
- Every example under `spices/plot/` and the guide compiles against the
  new constructors.
- Removing the deprecated `__make-renderer` export (in a later release)
  is the only follow-up; nothing in-tree depends on it after P4.

### P5 — Tests, negative fixtures, image round-trip

**Tasks**
- Per-kind: keep the existing pixel-content assertions; add a
  construct→render→pixel-count check per `*R` struct.
- Add negative fixtures (gated on the P5 negative-fixture harness from the
  uplift plan, if available; otherwise a `tur check`-expected-fail script):
  - wrong field type in a constructor is a type error, not a slot mishap;
  - a non-`Renderer` value in the renderer vec is rejected.
- Wire all of the above into the spice's `tests/` so CI covers them.

**Acceptance**
- `tur test spices/plot/tests` green; pixel output identical to pre-plan.
- Negative fixtures fail to compile with clear diagnostics.

### Dependency graph

```
P0 (de-monolith C) ─► P1 (BBox/RenderCtx) ─► P2 (structs + class)
                                                   │
                                                   ▼
                                              P3 (existential box)
                                                   │
                                                   ▼
                                              P4 (migrate ctors) ─► P5 (tests)
```

P0 and P1 are pure refactors and ship on their own. P2 blocks on P1; P3
blocks on P2; P4/P5 close it out.

---

## Risks and open questions

1. **Existential boxing support.** P3 assumes the same existential
   facility ECS used in PR #17 works for a `Renderer` dictionary outside
   the ECS world machinery. If it does not generalize, this is the one
   place the plan could hit a compiler dependency — *pause and file a
   report under `docs/reported/` in turmeric per CLAUDE.md* rather than
   forcing it. P0–P2 still deliver value (named fields, killed draw
   switch) even if the vec stays a `Vec` of int handles bridged at the
   boundary.

2. **Pixel-identity regression budget.** The draw code is sensitive
   (adaptive subdivision thresholds, tick rounding). The plan's
   verbatim-move discipline in P0 is what protects this; review P0 as a
   pure cut-paste with no edits to arithmetic.

3. **Field-count ergonomics.** Some kinds have many fields
   (`StackedHistR`). `make-struct` positional construction gets unwieldy;
   consider whether a small builder is worth it (this overlaps U6 typed
   variadic builders — defer unless it blocks P2).

4. **`text`/`label` ownership.** `__make-renderer` deep-copies `label`
   and `text` (`core.tur:80-93`). The typed structs should hold `cstr`
   with the same ownership contract; confirm GC vs malloc lifetime when
   the struct replaces the C-owned copy.

---

## Acceptance (whole plan)

- `tur test spices/plot/tests` is green with byte-identical rendered
  output before and after.
- `plot-into-canvas` contains no `switch (r->kind)`; renderer dispatch is
  via the `Renderer` typeclass.
- Each renderer kind is a `defstruct` with named, typed fields; the
  generic `data1..5 / f1..6 / i1..4` slot bag is gone from the public
  constructor path.
- A negative fixture demonstrates that a mistyped renderer field or a
  non-`Renderer` vec element is a compile-time error.
