---
title: Track C U5 (HKT recursion for ASTs) — feasibility after recent turmeric fixes
category: Spice-uplift feasibility analysis
status: PARTIALLY UNBLOCKED — AST-shaped targets are doable; json target is blocked + moot; one new compiler limitation to file
reported-by: turmeric-spices Claude (Track C, branch claude/track-c-u5-turmeric-2miu28)
verified-on: turmeric 0.22.0, main @ 15cf5fe (post #444 end-to-end monomorphization, #468 HKT-via-match-scrutinee), built from source (build-release)
---

# Track C U5 — can it be done after the recent turmeric fixes?

**Short answer: partially.** The higher-kinded-recursion machinery the U5
plan depends on (typed sum functors, `Functor`/`fmap`, `Fix`, recursive
`cata`) now compiles **and runs** for the AST-shaped targets whose child
positions are sub-nodes (c-dsl, glsl, scscm, regex, template). The
**first-listed target — `json` — is both blocked by a genuine compiler
limitation and moot** (the spice has no Turmeric-level recursive IR to
collapse).

U5 plan reference:
`rjungemann/turmeric:docs/upcoming/spices-type-features-uplift-plan.md`
§ "Phase U5 -- HKT recursion for ASTs" (targets: json, c-dsl, glsl,
scscm, regex, template).

All claims below were checked against a from-source `tur` 0.22.0
(`cmake --build build-release`) on turmeric main @ `15cf5fe`.

---

## What the recent fixes unblocked (verified working)

The Track A end-to-end monomorphization land (#444) and the HKT
instance-method fixes (#468 "HKT instance-method spec emitted when consumed
by a match scrutinee", #438 Applicative `ap` fn-type preservation) make the
core recursion-schemes pattern work end to end:

1. **Typed sum functor + `Functor` instance compiles and runs.** A
   `(defdata ExprF [p a] (LitF :int) (AddF a a) (MulF a a))` with
   `(definstance Functor [(ExprF P)] (fmap [c g] (match c ...)))` checks
   clean, and an F-algebra over it (`(ExprF int int) -> int`) runs.

2. **Recursive `cata`/fold over a `Fix`-wrapped AST runs in pure
   Turmeric** — no inline C for the recursion. A `+`/`*` evaluator built
   from `roll`/`unroll` (stdlib `Fix`) folded `(add (add (lit 1) (lit 2))
   (lit 10))` to `13` correctly. This is the actual U5 deliverable shape
   for ASTs whose children are sub-nodes.

   This is a real step beyond the `tests/fixtures/hkt-fix-cata` fixture in
   turmeric: that fixture keeps the node construction and the fold in
   **inline C** (`__opt_some`, a C `while` loop for `cata-nat`). The probe
   here does the fold in Turmeric.

3. `stdlib/fix.tur` (`Fix`/`roll`/`unroll`/`cata`/`ana`) and
   `stdlib/typeclass-functor.tur` (`Functor [^f]`) ship and load.

So **c-dsl `Expr`/`Stmt`, glsl, scscm `SExpr`, regex `Re`, and template
node IR are unblocked**: their AST nodes carry children as the recursive
type itself (bare type-var positions), which is exactly what works.

---

## Ergonomic warts (workable, worth noting)

- **`stdlib/Fix` is still int-carrier.** `(defdata Fix [^f] (Roll :int))`,
  `roll : [int] -> int`, `unroll : [int] -> int`. The fold therefore
  threads through an `(:: (unroll fix) (ExprF int int))` ascription rather
  than a fully by-value typed `Fix`. A by-value `Fix` would be
  `(defdata Fix [^f] (Roll (f (Fix f))))` — but that is rejected by the
  blocker below.

- **Single-param `defdata` reports kind `*`, not `* -> *`.**
  `(definstance Functor [ExprF])` on a single-param `(defdata ExprF [a] ...)`
  fails with `TUR-E0012: kind mismatch ... provides a kind-'*' type for
  parameter 1 which expects kind '* -> *'`. `defstruct`/`defopaque`
  single-param types do *not* have this problem (`Functor [Schema]`,
  `Functor [Backtrack]` work). Workaround: add a phantom first param and
  use a partial-application head, `(defdata ExprF [p a] ...)` +
  `(definstance Functor [(ExprF P)] ...)`. Mildly ugly but functional.

---

## The blocker: `defdata` constructor fields cannot hold applied types

A `defdata` constructor field must be a primitive keyword (`:int`, `:bool`,
`:cstr`, ...) or a bare type variable. **Any applied type constructor in a
field is rejected**, whether parametric or concrete:

    (defdata Nest [a] (N (Wrap a)))            ; error: field type must be a keyword
    (defdata ArrNode [a] (Arr (Box a)))        ; error
    (defdata JsonNode (JArr (list JsonNode)))  ; error (even concrete!)
    (defdata Fix [^f] (Roll (f (Fix f))))      ; error (blocks by-value Fix)

Diagnostic: `error: defdata: constructor field type must be a keyword like
:int, :bool, :cstr`.

This is the gating limitation for the U5 **json** node, whose plan shape is

    JsonF a = Null | Bool b | Num n | Str s | Arr (list a) | Obj (map str a)

The `Arr (list a)` and `Obj (map str a)` arms need a sum constructor that
carries a `(list a)` / `(map str a)` field by value — exactly what `defdata`
rejects today. (regex's `ReF` and the SExpr/c-dsl shapes mostly dodge this
because their children sit in bare recursive positions, e.g. `Alt a a`,
`Star a`; a target that needs `(list a)` children — e.g. a variadic
n-ary node — would hit the same wall.)

This is **not** addressed by the recent monomorphization / HKT PRs (#444,
#468, #469, #470, #471). It is a parser/elaborator restriction on
`defdata` field types and should be filed as a turmeric report (see below).

---

## The json target is also moot

Independent of the blocker: the `json` spice is **100% yyjson-backed**.
`spices/json/src/json/{parse,emit,encode,decode,patch}.tur` are inline-C
wrappers over `yyjson_*`; there is **no Turmeric-level recursive node IR**
(`grep` for `defdata`/`deftype`/`Fix`/`cata`/`unroll` over `spices/json/src`
finds only doc comments about `derive-json-sum`, no recursive node type).
`emit.tur` is a single `yyjson_write` call, not the "manual recursion in
`json__emit.c`" the U5 plan describes — that premise is stale.

So even if the `defdata` applied-field limitation were lifted, there is
nothing in the json spice to convert to a `cata`: the recursion lives in
yyjson's C, which we are not going to replace with a slower pure-Turmeric
tree walk. **json should be dropped from U5** (or the plan re-scoped to
"document that the yyjson backend already subsumes the manual-recursion
goal").

---

## Recommendation, per U5 target

| Target   | Status | Notes |
|----------|--------|-------|
| json     | **Drop / moot** | yyjson-backed; no Turmeric IR; also hits the applied-field blocker if attempted |
| c-dsl    | **Doable now** | `Expr`/`Stmt` children are sub-nodes (bare recursive positions); int-carrier `Fix` + recursive `cata` verified to work for this shape |
| glsl     | **Doable now** | same shape as c-dsl |
| scscm    | **Doable now** | `SExpr` children are sub-nodes |
| regex    | **Doable now** | `ReF a = Lit c \| Alt a a \| Concat a a \| Star a` — all bare recursive positions |
| template | **Doable now** | node IR via bare recursive positions |

Any individual node that needs a `(list a)`/`(map str a)` child (a
variadic/keyed node) is blocked until the `defdata` field limitation is
lifted; restructure such nodes as right-nested binary cons (`Concat a a`)
to stay within what works, or wait on the compiler fix.

P4 `match-fix` sugar is still unlanded but is explicitly ergonomics-only;
U5 ships with a verbose `cata` first (confirmed — the verbose form works).

---

## turmeric report to file (from a turmeric-rooted session)

This session is rooted in `turmeric-spices` and cannot write to
`rjungemann/turmeric`. The following should be filed under
`docs/reported/` there:

**`defdata` constructor fields reject applied type constructors.** A sum
constructor cannot carry a `(list a)`, `(map str a)`, `(Wrap a)`, or even a
concrete `(list JsonNode)` field — only primitive keywords and bare type
variables are accepted. This blocks (a) by-value typed `Fix`
(`(Roll (f (Fix f)))`) and (b) any U5 AST node with container-typed
children (the json `Arr`/`Obj` arms). Repro:

    (defdata Nest [a] (N (Wrap a)))
    ;; error: defdata: constructor field type must be a keyword like :int, :bool, :cstr

Secondary, lower priority: single-param `defdata` reports kind `*` (so
`(definstance Functor [SingleParamSum])` fails kind-check), while
`defstruct`/`defopaque` single-param types report `* -> *`. Workaround is a
phantom param + partial-app head; a fix would make defdata kind reporting
consistent with the other type formers.
</content>
</invoke>
