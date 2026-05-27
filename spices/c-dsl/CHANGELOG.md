# Changelog

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
