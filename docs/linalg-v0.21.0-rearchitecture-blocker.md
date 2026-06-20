---
title: linalg is blocked on a rearchitecture against turmeric v0.21.0
category: Spice-uplift blocker (Track C / U4 prerequisite)
status: IN PROGRESS — linalg/small migrated (proven :copy template); 5 modules remain
verified-on: turmeric 0.21.0, main @ 48e99d9 (post #462/#463/#464; built from source)
verified-by: turmeric-spices Claude (Track C, branch claude/track-c-turmeric-3ghmwl)
---

# linalg — blocked on rearchitecture for v0.21.0

## Progress (2026-06-20)

- **`linalg/small` is migrated and verified** — it is the self-contained,
  fixed-size module (`vec2/3/4`, `mat2/3/4`, the mat4 graphics ops). It now uses
  `:copy` struct *values* with `make-struct` + `(.field v)`, `^mut` locals for
  build-by-mutate, libm via `extern-c [x : float] : float`, and inline-C
  address-of (`&(m.m00)`) for the OpenGL `*-ptr` helpers. Checks clean and a
  cross-module smoke test (constructors, `vec3-cross`, `mat4-mul`,
  `mat4-mul-vec4`, `mat4-ptr`) compiles+runs. This is the **proven `:copy`
  template** for any other fixed-size struct work. Two notes: the constructor
  names moved `vecN` → `vecN-of` (the type name `vecN` is now the make-struct
  tag and can't double as a function), and `mat4-inv` is **stubbed with a
  `panic`** — its old body mixed `vec3-cross`/`vec4-dot` on the same values
  (only valid under int-pointer aliasing) and needs a type-correct reimpl + a
  numerical test (TODO(linalg-u4)).
- **Remaining (5 modules):** `vec`/`mat` (core, need the `(Vec float)` + `^borrow`
  dynamic model), `solve`/`decomp`/`fmt` (same, plus the pre-existing paren
  bugs). These are the larger rewrite; `small` did not need `Vec`/borrows.

## Additional v0.21.0 idiom changes found during the `small` migration

Beyond `sizeof` / accessor-functions / `float64*` (below), the rewrite also hit:
`(float x)` coercion removed (type params `: float` instead, or `int->float`);
unary `(- x)` and reciprocal `(/ x)` removed (use `(- 0.0 x)` / `(/ 1.0 x)`);
struct field mutation needs a `^mut` binding; `defstruct Foo` reserves `Foo` as
the constructor tag (collides with a same-named `defn`); `(error ...)` is not a
turmeric form (use `(panic ...)`); a `^borrow :copy` struct is passed *by value*
in inline C (`v.x`, not `v->x`). These belong in the Report #2 migration note.

---

`linalg` does not build against turmeric v0.21.0. While paying down the Track C
type-hygiene work, an attempt was made to migrate it (rename the colliding
`vec-*` API to `la-vec-*`, swap `declare`→`extern-c`, migrate `defstruct`
syntax). That migration is **necessary but not sufficient**: two further
v0.21.0 changes are fundamentally incompatible with how linalg is written, and
fixing them is a rewrite, not a migration. The migration diff was therefore
**not committed** — linalg is left untouched at its prior (already-broken) state
so nothing regresses.

linalg stores every struct pointer as `:int` and hand-rolls allocation and
field access. v0.21.0 removed the two language features that model rests on.

## The five breaks (verified individually against main @ 48e99d9)

### Mechanical (a migration *can* fix these)

1. **`(declare …)` is no longer a valid form** → use `(extern-c …)`. linalg
   declares `malloc`/`free`/`printf`/`snprintf` this way in 5 files.
2. **Bare `defstruct` field syntax is rejected** — `(defstruct mat rows :int …)`
   must become `(defstruct mat [rows : int …])`.
3. **The public `vec-*` API hard-collides with the auto-loaded `stdlib/vec.tur`
   prelude** (`vec-new vec-len vec-get vec-set! vec-free`, and the
   `(defstruct vec …)` accessors `vec-len`/`vec-data`). Resolved by renaming the
   `linalg/vec` module API + type to `la-vec-*` / `la-vec` (mat-*, vec2/3/4 are
   unaffected).

### Architectural (a migration *cannot* fix these — full rewrite required)

4. **`sizeof` is no longer a Turmeric-level operator.** `(malloc (sizeof mat))`,
   `(sizeof :int)`, `(sizeof :float)` now error (`unknown function or operator
   'sizeof'` / `keyword in expression position requires -Xsymbols`). In v0.21.0
   `sizeof` exists only inside ```c blocks. linalg uses it pervasively to size
   its heap allocations.

5. **`defstruct` no longer generates `Struct-field` accessor *functions*; field
   access is `(.field obj)` on a typed struct value, and the type name is now a
   `make-struct` constructor.** Verified:
   ```turmeric
   (defstruct pt [x : int  y : int])
   (.x p)      ;; OK
   (pt-x p)    ;; error: unknown function or operator 'pt-x'
   ```
   linalg's entire surface is built on the accessor-function idiom over `:int`
   pointers — `(mat-rows m)`, `(set! (mat-rows m) v)`, `(vec-data v)`. None of
   these resolve anymore. In `small.tur` this also surfaces as
   `defstruct: 'vec2' is already defined`, because the new `vec2` constructor
   collides with the file's own `(defn vec2 …)`.

### Also: pre-existing, predates v0.21.0

`solve.tur`, `decomp.tur`, and `fmt.tur` carry **paren imbalances that the
reader rejects** (verified on the HEAD versions: `solve.tur:177 unterminated
list`, `decomp.tur:185 unterminated list`, `fmt.tur:188 unexpected ')'`). These
files have never parsed standalone; they are independent of v0.21.0 and must be
repaired as part of any linalg work regardless.

## Why this isn't a Track C "rename" ticket

Making linalg build requires rewriting every function to use typed struct values
+ `.field` access + `make-struct`, moving the raw `float64*` data arrays behind
inline-C (or a `:heap` typed pointer), and reworking the `:int`-pointer model
end-to-end. That is the U4 (sized-types) rearchitecture, not the U4
*prerequisite* rename. Nothing in the repo imports linalg, so it is not blocking
any other spice — but it cannot be made green by the Track C transformation set
alone.

## Options (for the owner to choose)

1. **Dedicated linalg rearchitecture task** — rewrite to the v0.21.0 typed-struct
   model (`.field` + `make-struct` + inline-C for the data arrays), folding in
   the mechanical migration above and the paren fixes. Largest, cleanest.
2. **turmeric-side compatibility decision** — if `sizeof` and/or accessor-function
   generation are intended to survive (other spices may rely on them), that is a
   compiler conversation to have in `rjungemann/turmeric`, not here.
3. **Defer** — leave linalg as-is (it is already red and unimported); revisit when
   U4 (sized linalg: `Vec n`/`Mat m n`) is actually scheduled, since that phase
   will rewrite these signatures anyway.

The spec-conformant mechanical migration (items 1–3 + the `mat_test.tur:136`
paren fix) was produced and verified clean per-file, but is held uncommitted
pending the rearchitecture decision so the branch carries no non-building code.
