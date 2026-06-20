---
title: Residual soft blockers found during Track C plot paydown (2026-06-20)
category: Spice-uplift soft blockers — to be filed as turmeric reports
status: OPEN
reported-by: turmeric-spices Claude (Track C, branch claude/track-c-turmeric-xaky6y)
verified-on: turmeric 0.21.0, main @ bb9d318 (post #460 / #461; built from source)
---

# Residual soft blockers (Track C plot paydown)

While paying down the S4–S7 workarounds in `spices/plot/src/plot/core.tur`
(now that #460/#461 fixed the self-recursive-`defn` carrier-typing and the
pass-by-ptr struct-param return), two soft workarounds **could not** be
retired. They are filed here so a report can be raised against `rjungemann/turmeric`
later (this session is rooted in turmeric-spices and cannot write to turmeric).

Neither blocks: both have a one-line workaround already in the landed spice
code. They are quality-of-life gaps, same flavor as S4/S6.

---

## W1 — `letrec` self-recursive closure with a by-value struct / `(Vec T)` accumulator types its own call at the int carrier

**Severity:** Low/Medium. Type-check fails (`if` branch mismatch); the
workaround is to ascribe the recursive call `(:: (go ...) T)`, OR to hoist the
loop to a top-level recursive `defn` (which #460 fixed). Does not block.

**Relationship to fixed work:** This is the `letrec`-closure analogue of S6
(`self-recursive-call-typed-at-carrier-int`, fixed by #460). #460's `RR1` fix
propagates the declared result shape to the forward-decl self-binding for a
top-level `defn`; a `letrec`-bound self-recursive `fn` goes through a different
binding path and is **not** covered, so its self-call still collapses to the
int64 carrier whenever the threaded accumulator is a carrier-lowered type
(`:copy` struct, `(Vec T)`, handle).

Note S5 (`letrec-self-recursive-closure-misemits-undeclared-binding`) is
genuinely fixed — the *codegen* for a letrec self-call is correct now. W1 is a
distinct, earlier-stage (elaboration/typing) gap that bites before codegen.

### Reproduction (verified on this tree)

```turmeric
(defstruct Box :copy [lo : int  hi : int])
(defn mk [n : int] : Box (make-struct Box n n))

;; top-level defn with a struct accumulator: OK (S6 / #460 fixed this)
(defn go-defn [n : int  i : int  acc : Box] : Box
  (if (>= i n) acc (go-defn n (+ i 1) acc)))

;; same shape as a letrec-bound self-recursive closure: FAILS
(defn go-letrec [n : int] : Box
  (letrec [go (fn [i : int  acc : Box] : Box
                (if (>= i n) acc (go (+ i 1) acc)))]
    (go 0 (mk 7))))
```

```
error: if branches have mismatched types: then=Box else=int
  (the `else` is the recursive `(go ...)` call, declared : Box, typed int)
```

`(Vec int)` accumulator reproduces identically (`then=(type-app Vec int) else=int`).

### Workaround (verified)

Ascribe the recursive call — `(:: (go (+ i 1) acc) Box)` — then both `check`
and `run` succeed (the S5 codegen fix carries it through). Or hoist to a
top-level `defn` (what the plot spice does).

### Spice-side impact

`spices/plot/src/plot/core.tur`: `__renderers-bbox-go` and `__any-to-legacy-go`
stay top-level `defn`s rather than `letrec` closures local to `renderers-bbox` /
`anyrenderers->legacy`. With #460 the `defn` form needs no ascription and threads
a real `BBox` accumulator (the former raw `{x0,x1,y0,y1;valid}` scalar-carrier
workaround is gone). Folding them into local `letrec` closures — the more
idiomatic form — is blocked on W1.

### Fix direction

Extend #460's `RR1` declared-result-shape propagation to cover the
`letrec`-bound self-recursive `fn` binding, so its self-call resolves to the
closure's declared return rather than the int64 carrier.

---

## W2 — a bare `(vec-new)` passed directly as a concrete `(Vec T)` argument does not unify its element tyvar

**Severity:** Low. One-token ascription at the call site is the workaround.
Does not block.

**Relationship to fixed work:** S4 (`vec-new-vec-push-element-type-not-inferred`,
fixed by #461) back-propagates a concrete element type onto a **local let-bound
`vec-push!` receiver**. It does **not** cover a fresh `(vec-new)` handed
straight to a function parameter whose type is a concrete `(Vec T)` — there is
no `vec-push!` on it in the caller to pin the element, and ordinary argument
unification leaves the element tyvar open.

### Reproduction (verified on this tree)

```turmeric
(defn sink [v : (Vec int)] : int (vec-len v))
(defn main [] : int (sink (vec-new)))   ;; bare vec-new as a (Vec int) arg
```

```
error [TUR-E0001]: function 'sink' arg 1:
  expected (type-app Vec int), got (type-app Vec tyvar 'A')
```

### Workaround (verified)

Ascribe at the call site: `(sink (:: (vec-new) (Vec int)))` — checks clean.

### Spice-side impact

`spices/plot/src/plot/core.tur`, `anyrenderers->legacy`: the seed accumulator
stays `(:: (vec-new) (Vec int))` rather than a bare `(vec-new)`.

### Fix direction

When a fresh `(vec-new)`-typed value with an unresolved element tyvar is unified
against a concrete `(Vec T)` parameter at a call site, bind the element tyvar to
`T` (the same forward-flow #461 added for the `vec-push!` receiver case, applied
at argument unification).

---

## Also retired this pass (was previously a documented spice workaround)

`spices/json/src/json/encode.tur` (`derive-json-sum-decode` / `derive-json-sum`)
used to route its `(ok <adt>)` tail through a generated standalone `defn`
because a typeclass method whose tail was a bare `(ok <adt-value>)` mis-boxed
the ADT-typed result slot (the value-struct case had been fixed earlier, the
sum/ADT case not). On this turmeric tree the inline box is correct, so the
delegation was removed and the `Decode` instance boxes `(ok ...)` directly in
the method body. Verified: the full json test suite (15 files) is green,
including `round-trip-sum`, `derive-decode-sum`, `derive-encode-sum`. No
turmeric report needed — this one is closed, not open.
</content>
