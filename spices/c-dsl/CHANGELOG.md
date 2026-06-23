# Changelog

## Unreleased

### Added

- `c-dsl/ir`: a Fix-encoded IR for c-dsl, with one generic catamorphism
  (`node-cata`) that every fold is expressed through. The pretty-printer
  (`node->c`) is a single `cstr`-carrier F-algebra over that driver -- no
  per-node scaffolding -- and reproduces the existing flat builders'
  (`c-binop`, `c-if`, ...) output byte-for-byte. A second `int`-carrier fold
  (`node-size`) shares the same driver. Smart constructors `ce-*` (expressions)
  and `cs-*` (statements) build `CNode` trees. This is the U5 "Fix-encoded IR
  uplift" surface (turmeric plan
  `docs/upcoming/v1/u5-c-dsl-glsl-fix-encoded-ir-plan.md`); the existing flat
  builders are unchanged and remain the public surface.

## 0.2.0

### Breaking changes

- `c-stmts` now takes a `Vec[cstr]` as its `stmts` argument instead of
  a cons list. Build with `(vec-of stmt1 stmt2 ...)`.

  Before:

  ```turmeric
  (c-stmts (cons (c-return "0") 0))
  ```

  After:

  ```turmeric
  (c-stmts (vec-of (c-return "0")))
  ```

- `c-fn-type` now takes a `Vec[cstr]` as its `param-types` argument
  instead of a cons list.

  Before:

  ```turmeric
  (c-fn-type (cons ":int" (cons ":int" 0)) ":int")
  ```

  After:

  ```turmeric
  (c-fn-type (vec-of ":int" ":int") ":int")
  ```

### Added

- `c-join-vec`: like `c-join` but operates on `Vec[cstr]`. Exported
  from `c-dsl/codegen`. The cons-list `c-join` is unchanged.

### Note

`c-block`, `c-lines`, `c-join`, and `compile-c` still take cons lists.
Only `c-stmts` and `c-fn-type` were flagged for migration in this
release.

## 0.1.0

- Initial release.
