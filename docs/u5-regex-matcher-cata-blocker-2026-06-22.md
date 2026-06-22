---
title: Track C U5 (regex matcher as one cata) — re-checked after turmeric #489..#497
category: Spice-uplift feasibility analysis (Track C / U5)
status: PARTIALLY UNBLOCKED — function-typed-carrier cata now works for scalar-argument carriers (#489); the regex matcher carrier (a function whose own argument is a function) still mis-lowers and segfaults. Matcher stays direct recursion. New minimal repro filed below.
verified-on: turmeric main @ d9fb741 (#497, built from source: cmake --build build-release)
verified-by: turmeric-spices Claude (Track C, branch claude/track-c-turmeric-work-6471kr)
plan: rjungemann/turmeric docs/upcoming/spices-type-features-uplift-plan.md (Phase U5)
---

# U5 regex matcher: can it become one `cata` now?

## TL;DR

The U5 feasibility doc (`u5-hkt-ast-feasibility-2026-06-21.md`) closed with one
open edge: the regex spice's backtracking matcher wanted to be a single
`re-cata` with a **function-typed carrier** `B`, but `(re-cata match-alg e)`
"came back as `int` and was not callable", so `re-matches?` shipped as direct
structural recursion.

Re-checked against turmeric main built from source at `d9fb741` (post the
#479..#497 cluster, which includes **#489 "hkt-cata function-typed carrier"**):

- **That old blocker is gone.** A function-typed-carrier cata now type-checks,
  compiles, *and runs* when the carrier's arguments are **scalars**:
  `B = (fn [int] int)` (turmeric fixture `hkt-cata-fn-carrier-recursive`) and
  `B = (fn [int int] int)` both fold and return the right value.
- **A new, narrower blocker remains.** The regex matcher carrier is
  `B = (fn [(fn [cstr] bool) cstr] bool)` — its **first argument is itself a
  function** (the CPS continuation `k`). That case still mis-lowers: at the
  carrier-result application site the function-typed argument is passed *thin*
  (cast to `int64_t`) instead of as a fat closure box, so the program
  **segfaults at runtime** (it now type-checks and compiles cleanly — only
  codegen is wrong).

So the matcher-as-cata is still blocked, but on a different, sharper defect
than recorded on 2026-06-21. `re-matches?` stays direct recursion.

## What *did* land this pass

- The layer ascriptions the value folds and `unroll-re` used to need
  (`(:: l (ReF Re))` after `(match e (Roll l) ...)`) are **no longer
  required** — matching `(Roll l)` now refines `l`'s recursion slot directly.
  `spices/regex/src/regex/tree.tur` dropped them; the 14/14 `tests/tree_test`
  suite stays green.

## Verification matrix (turmeric main @ d9fb741, `tur` built from source)

| Carrier `B` | type-checks | compiles (`cc`) | runs | result |
|---|---|---|---|---|
| `(fn [int] int)` (turmeric fixture) | ✅ | ✅ | ✅ | 7 / 12 |
| `(fn [int int] int)` (2 scalar args) | ✅ | ✅ | ✅ | 13 |
| `(fn [(fn [int] int) int] int)` (fn-typed **arg**) | ✅ | ⚠️ warns | ✗ | **segfault** |
| regex matcher `(fn [(fn [cstr] bool) cstr] bool)` | ✅ | ⚠️ warns | ✗ | **segfault** |

The two failing rows are the *same* defect: a carrier whose parameter list
contains a function type. Arity is not the trigger (the 2-scalar-arg row
passes); a function-typed *argument* is.

## Minimal repro (no inline C)

```turmeric
(load "stdlib/typeclass-functor.tur")

(defdata ExprF :copy [a] (LitF :int) (AddF a a))
(defdata Expr  :copy (Roll (ExprF Expr)))

(definstance Functor [ExprF]
  (fmap [c g]
    (match c
      (LitF n)   (LitF n)
      (AddF x y) (AddF (g x) (g y)))))

(defn unroll-e [e : Expr] : (ExprF Expr) (match e (Roll l) l))

(defn cata [B] [alg : (fn [(ExprF B)] B) e : Expr] : B
  (alg (:: (fmap (unroll-e e) (fn [c : Expr] : B (cata alg c))) (ExprF B))))

(defn lit [n : int] : Expr (Roll (LitF n)))
(defn add [x : Expr y : Expr] : Expr (Roll (AddF x y)))

;; carrier B = (fn [(fn [int] int) int] int): first arg is itself a function
(defn alg [l : (ExprF (fn [(fn [int] int) int] int))]
         : (fn [(fn [int] int) int] int)
  (match l
    (LitF n)   (fn [k : (fn [int] int) s : int] : int (k (+ s n)))
    (AddF x y) (fn [k : (fn [int] int) s : int] : int
                 (x (fn [s2 : int] : int (y k s2)) s))))

(defn main [] : int
  (println ((cata alg (add (lit 3) (lit 4))) (fn [r : int] : int r) 0))) ;; expect 7
```

- `tur check` → exit 0.
- `tur run` → C compile emits an int-from-pointer warning at the carrier-result
  application site and the program **segfaults**. The generated call is

  ```c
  __t56 = ((int64_t (*)(void*, int64_t, int64_t))(intptr_t)(...))(
              (void*)(...), __fn_1078, INT64_C(0));
  ```

  i.e. the continuation argument `__fn_1078` (a fat closure pointer) is passed
  in an `int64_t` slot and the callee dispatches it thin → jump into the env
  block.

Control that **passes** (swap the function-typed first arg for a scalar):

```turmeric
(defn alg [l : (ExprF (fn [int int] int))] : (fn [int int] int)
  (match l
    (LitF n)   (fn [a : int b : int] : int (+ n (+ a b)))
    (AddF x y) (fn [a : int b : int] : int (+ (x a b) (y a b)))))
;; ((cata alg (add (lit 3) (lit 4))) 1 2) => 13
```

## Relationship to the existing turmeric report

`rjungemann/turmeric:docs/reported/hkt-cata-function-carrier-recursive-segfault.md`
records the recursive function-carrier segfault as **FIXED** for its canonical
repro (`B = (fn [int] int)`). The case above is the **next** variant: the
carrier's *argument* is a function. It needs the same fat-closure ABI treatment
#489 gave the carrier *result* to be extended to function-typed *arguments* of
the carrier at the application site. Filing it against turmeric requires a
turmeric-rooted session (writes to turmeric are out of scope here); the
self-contained repro above is ready to drop into `docs/reported/`.

## Bottom line for Track C / U5

- regex value folds: already one `re-cata` each (shipped earlier).
- regex matcher: **still direct recursion** — blocked on the function-typed
  *argument* carrier defect above, not on anything spice-side.
- The other U5 AST targets (`c-dsl`, `glsl`, `scscm`, `template`) do **not**
  need a function-typed carrier and were already assessed doable post #483/#487;
  this defect does not gate them.
