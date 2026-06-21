---
title: Track C U5 (HKT recursion for ASTs) — feasibility after recent turmeric fixes
category: Spice-uplift feasibility analysis
status: UNBLOCKED — defdata applied-field blocker fixed by turmeric #483 (+#482); U5 AST targets are now doable. json target remains moot (yyjson-backed). Minor annotation-level edges remain.
reported-by: turmeric-spices Claude (Track C, branch claude/track-c-u5-turmeric-2miu28)
verified-on: turmeric 0.22.0, main @ 99cc8b3 (post #483 "Allow applied type constructors in defdata constructor fields", #482 by-value parametric struct fields), built from source (build-release)
---

# Track C U5 — can it be done after the recent turmeric fixes?

**Short answer: yes, the blocker is fixed.** The `defdata` applied-field
limitation reported on 2026-06-21 was resolved the same day by turmeric
**#483** ("Allow applied type constructors in defdata constructor fields",
main @ `99cc8b3`), together with **#482** (by-value parametric struct field
layout). The single-param `defdata` kind wart is gone too. The full
recursion-schemes pattern U5 needs — typed sum functors, `Functor`/`fmap`,
a **by-value typed `Fix`**, and a recursive `cata` over container-typed
children — now compiles **and runs**.

The first-listed U5 target, `json`, remains **moot** for an unrelated
reason: the spice is yyjson-backed and has no Turmeric-level recursive IR
to collapse.

U5 plan reference:
`rjungemann/turmeric:docs/upcoming/spices-type-features-uplift-plan.md`
§ "Phase U5 -- HKT recursion for ASTs" (targets: json, c-dsl, glsl,
scscm, regex, template).

All claims below were checked against a from-source `tur` 0.22.0
(`cmake --build build-release`) on turmeric main @ `99cc8b3`.

---

## What now works (verified — checks AND runs)

1. **By-value typed `Fix`.** `(defdata Fix [^f] (Roll (f (Fix f))))` is
   accepted (was rejected pre-#483). No more int-carrier `Roll :int`
   indirection required.

2. **Sum functor + `Functor` instance, single param.**
   `(defdata ExprF [a] (LitF :int) (AddF a a) (MulF a a))` with
   `(definstance Functor [ExprF] (fmap [c g] (match c ...)))` checks clean.
   The earlier `TUR-E0012` kind-`*` error on single-param `defdata` is
   fixed — **no phantom-param / partial-app-head hack needed**.

3. **Recursive `cata` over a by-value `Fix`, in pure Turmeric.** An
   arithmetic evaluator (`AddF`/`MulF` children as sub-nodes) folded
   `(mul (add (lit 1) (lit 2)) (lit 10))` to `30`.

4. **Container-typed children (the U5 json `Arr`/`Obj` shape).** A node
   `(defdata JsonF [a] (JNumF :int) (JArrF (Vec a)))` with a `Functor`
   instance, wrapped in the by-value `Fix`, folded a nested array
   `[10, 20, [5, 7]]` to `42` via a `cata` that sums all numbers.

So **every U5 AST target is unblocked**: c-dsl `Expr`/`Stmt`, glsl, scscm
`SExpr`, regex `Re`, template node IR — including nodes whose children sit
in a container, not just bare recursive positions.

---

## Residual edges (annotation-level, not blockers)

1. **Use `Vec`/`Map`, not the `list`/`option` aliases, for container
   children.** `(JArrF (Vec a))` and `(... (Map str a))` are accepted;
   `(JArrF (list a))` / `(... (option a))` fail with `TUR-E0012: cannot
   apply a type of kind '*' as a type constructor`, because `list` and
   `option` are macro-aliases (nested `tcons` / tagged structs), not
   arrow-kinded type constructors. The U5 plan writes the json node as
   `Arr (list a) | Obj (map str a)`; re-spell those as `Vec`/`Map`.

2. **Matching `(Roll layer)` does not refine the layer's recursive type
   var.** After `(match e (Roll layer) ...)`, `layer` infers with its
   recursion slot unresolved (children come back as `int`). Add one
   ascription per fold: `(let [l (:: layer (ExprF (Fix ExprF)))] (match l
   ...))`. With it, the fold checks and runs.

3. **Generic `vec-get` returns the element at the int carrier.** When a
   child lives in a `(Vec (Fix F))`, ascribe the read:
   `(:: (vec-get xs i) (Fix F))`. One ascription per child access.

4. **Bare 0-arg named type as a field is still rejected.** `(Cx C)` (a
   non-applied named type) still triggers the old "field type must be a
   keyword" message. Recursion through `(Fix F)` / applied forms sidesteps
   this, so it rarely bites; worth a follow-up if a spice wants flat
   mutually-recursive node types without `Fix`.

None of these block U5; they are the verbose-`cata` ergonomics the plan
already anticipated (P4 `match-fix` sugar would erase #2/#3).

---

## The json target is still moot

Independent of the (now-fixed) compiler blocker: the `json` spice is
**100% yyjson-backed**. `spices/json/src/json/{parse,emit,encode,decode,patch}.tur`
are inline-C wrappers over `yyjson_*`; there is **no Turmeric-level
recursive node IR**. `emit.tur` is a single `yyjson_write` call, not the
"manual recursion in `json__emit.c`" the U5 plan describes — that premise
is stale. There is nothing in the json spice to convert to a `cata`; the
recursion lives in yyjson's C, which we should not replace with a slower
pure-Turmeric tree walk. **json should be dropped from U5** (or the plan
re-scoped to note the yyjson backend already subsumes the goal). The
json-shaped functor itself is now expressible (verified above) — it just
has no home in the current json spice.

---

## Recommendation, per U5 target

| Target   | Status | Notes |
|----------|--------|-------|
| json     | **Drop / moot** | yyjson-backed; no Turmeric IR to collapse |
| c-dsl    | **Doable now** | `Expr`/`Stmt` via by-value `Fix` + `cata` |
| glsl     | **Doable now** | same shape as c-dsl |
| scscm    | **Doable now** | `SExpr` via by-value `Fix` |
| regex    | **Doable now** | `ReF a = Lit c \| Alt a a \| Concat a a \| Star a` |
| template | **Doable now** | node IR + container children via `Vec`/`Map` |

Suggested first landing: **regex** or **scscm** — smallest ASTs, children
mostly in bare recursive positions, so the residual ascriptions are
minimal. Ship the verbose `cata` first; P4 `match-fix` sugar is a later
ergonomics pass.

---

## Prototype landed: `regex/tree` (U5 proof-of-concept)

`spices/regex/src/regex/tree.tur` + `spices/regex/tests/tree_test.tur` are a
working U5 prototype (regex is PCRE2-backed with no Turmeric IR, so this is a
new self-contained recursion-schemes surface, not a refactor of the
bindings). It ships:

- `ReF a` sum functor + `Functor [ReF]` instance,
- a by-value fixed point `Re = Roll (ReF Re)` (enabled by #483),
- a generic `re-cata` (cata-via-`fmap`),
- folds `re-size` / `re-nullable?` / `re->str` and a backtracking
  `re-matches?`.

**14/14 tests green** against from-source `tur` @ `99cc8b3`.

Two things the prototype surfaced:

1. The generic `re-cata` (the U5 ideal — every fold is one F-algebra) **type
   checks but miscompiles at runtime**: boxed/mis-typed results at `int`/`bool`
   carriers (so `(= 6 (re-cata size-alg e))` is false) and a **segfault at the
   `cstr` carrier** (mis-cast `fmap` closure thunk). Filed as a turmeric report
   (see below). The shipped folds therefore use direct structural recursion —
   the same algebras, inlined — which is correct and fast.
2. The matcher must be direct recursion too: the pure-cata form pre-builds
   child matchers and captures them in a returned closure, which mis-emits the
   captured binding in C (closure-capture codegen gap, W3-adjacent).

So U5 on regex is **done and green today** via direct structural folds; the
generic-`cata` ergonomics wait on the codegen fix below.

---

## Status of the turmeric reports

- Blocker filed 2026-06-21 ("`defdata` constructor fields reject applied type
  constructors") — **RESOLVED by #483**; secondary single-param kind issue
  resolved too.
- **NEW, outstanding:** generic catamorphism via `Functor` `fmap` miscompiles
  at runtime (boxed `int`/`bool` results; `cstr`-carrier segfault from a
  mis-cast closure thunk). Repro in the regex prototype; to be filed under
  turmeric `docs/reported/hkt-fmap-cata-carrier-miscompile.md` from a
  turmeric-rooted session.
- Lower priority: closure-capture codegen gap when a returned closure captures
  `let`-bound folded closures (blocks the pure-cata matcher; W3-adjacent).

The layer `(:: ...)` and `vec-get` ascriptions remain annotation-level and do
not warrant a report.
