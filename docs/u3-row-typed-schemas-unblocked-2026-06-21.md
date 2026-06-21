---
title: Track C U3 (row-typed schemas) — compiler blockers retired by turmeric #479–#483
category: Spice-uplift blocker assessment (Track C / U3)
status: UNBLOCKED — every compiler prerequisite verified present; no spice code landed yet
verified-on: turmeric main @ 99cc8b32 (post #479/#480/#481/#482/#483; built from source)
verified-by: turmeric-spices Claude (Track C, branch claude/track-c-u3-turmeric-cgmo6u)
plan: rjungemann/turmeric docs/upcoming/spices-type-features-uplift-plan.md (Phase U3)
---

# U3 — row-typed schemas: unblocked as of turmeric main @ 99cc8b32

## TL;DR

Phase **U3 (row-typed schemas)** of the spices type-features uplift can be
done now. The type-level mechanism (P0 typed-field `#row{k : T ...}`
literals) shipped earlier; the remaining gap was the **runtime decoder
machinery** the U3 decoder model is built on — constrained-instance
`Encode`/`Decode` over containers, and struct/`defdata` fields that carry
parametric containers. A cluster of turmeric-main fixes (**#479, #480, #481,
#482, #483**) retires that gap. None of these are in the `v0.22.0` release
(tag @ `5318de7d`), so **U3 work must build `tur` from main**, not from a
release tarball.

There is **no remaining compiler blocker for U3.**

## What U3 is (from the plan)

> **Goal:** carry "what's in this row/header/column-set" at the type level
> using the same `#row{...}` phantom machinery the `Query` value uses in ECS.

Targets: `frame` (`Frame<#row{...}>`), `postgres`/`sqlite`
(`Result<#row{...}>`, `Stmt<params cols>`), `http`/`httpd`
(`Request<#row{headers...}>`), `json` (object shapes as rows). The plan's
**"U3 decoder model"** specifies that every backend's per-row decoder returns
`Result<T, Vec<DecodeError>>` with a shared `DecodeError` (path +
expected-type + got-tag) and validation-applicative error accumulation,
generated from the static row by a `derive-decoder` macro (sibling of P2a's
`derive-json`). The deliverable for U3 is a **negative fixture per target**:
code that *fails to compile* with a known-wrong row.

That decoder model is what needs the container/parametric machinery below.

## The blockers and the fixes that retire them

| U3 building block it enables | turmeric fix | commit |
|---|---|---|
| `Encode`/`Decode [Cons]` — dispatch a class method on a `:heap` list element | #479 | `12c6f31c` |
| `Decode [Option]` / nested container *construct* in a constrained instance body (`(ok (some …))`) | #480 | `eca8bc98` |
| constrained generic `defn` returning a parametric container | #481 | `15cf5fec` |
| `defstruct` field of a by-value parametric struct (`(Option cstr)`) — laid out inline, not collapsed to the int64 carrier | #482 | `e00a7104` |
| `defdata` constructor field that is an applied type constructor (`(Box a)`, `(Pair2 :cstr a)`, `(f (Fix f))`) | #483 | `99cc8b32` |

Why each matters for U3:

- **#479/#480/#481** are the read-side, construct-side, and return-side of
  the same carrier-ABI machinery: a constrained `Encode`/`Decode` instance
  over a *container* element type. The json `Decode` typeclass the U3 decoder
  model reuses (from P2a) needs all three to decode `Option`/list/nested-`Result`
  fields without falling back to the int64 carrier. Before these, json could
  *encode* optional/contained fields but not *decode* them (the two reports
  filed in turmeric #478, now closed by #479/#480).
- **#482** lets a `DecodeError` record (or any row-derived `defstruct`) carry a
  by-value parametric field such as `(Option cstr)` inline. Without it the
  field collapsed to `long int` and `make-struct` / `.field` mismatched.
- **#483** lets `DecodeError` be a sum type whose constructors carry applied
  types, and lets `Result<T, Vec<DecodeError>>`-shaped accumulators be
  expressed directly. It also unblocks the `Fix F` self-application U5 will
  want, but U3 needs it for the error-accumulator ADT.

## Verification (turmeric main @ 99cc8b32, `tur` built from source)

Built `tur` from a fresh clone of main at `99cc8b32`
(`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`).
All checks below were run against that binary.

| Check | Expected | Result |
|---|---|---|
| `Encode [Cons]` over `int`/`float`/`cstr` (the #479 repro) | `42` / `7.1` / `"hi"` | ✅ |
| `Decode [Option]` over `int`/`cstr` (the #480 repro) | `42` / `hi` | ✅ |
| `defstruct` field of `(Option cstr)`/`(Option int)` (turmeric fixture `defstruct-field-byvalue-parametric-struct`) | `1` / `0` / `42` / `9` | ✅ |
| `defdata` field of applied ctor (turmeric fixture `defdata-applied-type-field`) | `7` | ✅ |
| Typed-field row `(Tbl #row{id : int  name : cstr})` accept (turmeric fixture `typed-field-row-accept`) | `3` | ✅ |
| **U3 negative deliverable**: passing a `#row{uid:int label:cstr}` where `#row{id:int name:cstr}` is expected | fails to compile | ✅ `TUR-E0001` row-mismatch |

The first two repros previously produced a hard C compile error / silent
miscompile; they now compile and run correctly. The last row demonstrates the
U3 "negative fixture" deliverable already works: two rows with identical
element types but different field names refuse to unify.

## What is NOT yet done (scope of the actual U3 PRs)

This doc records only that the **compiler** side is ready. The spice-side work
remains, one PR per target per the plan's within-spice ordering
(opaques before rows for postgres/sqlite):

1. `frame` — `Frame<#row{...}>`; `col : Frame r -> (k in r) -> Col t`.
2. `postgres`/`sqlite` — `Result<#row{...}>`, `Stmt<params cols>` (after U1 opaques).
3. `http`/`httpd` — `Request<#row{headers...}>` / `Response<...>`.
4. `json` — object shapes as rows; retire the hand-rolled cons-walk decoder in
   `spices/json/src/json/encode.tur` in favor of the now-unblocked container
   `Decode` path.

Shared infrastructure to factor once (per the decoder model): a `DecodeError`
(path + expected-type + got-tag), a `Result<T, Vec<DecodeError>>` return
convention, and a `derive-decoder` macro that walks the static row at
elaboration time and emits the runtime decoder (mirroring `stdlib/schema.tur`'s
validation-applicative accumulation).

## Caveat

The mechanisms above were verified in isolation, not via a full `json`/`frame`
spice-suite build against this `tur`. Building the whole spice suite against
main is the natural first step of the first U3 PR.
