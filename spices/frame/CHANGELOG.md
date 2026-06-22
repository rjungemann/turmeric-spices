# Changelog

## Unreleased

### Added

- `frame/typed` -- a row-typed `Frame` newtype (Track C / phase U3). `Frame`
  carries its column-set as a phantom kind-`[*]` row argument over the raw
  `:int` frame handle, so a function pinned to a concrete schema
  (`(Frame #row{id : int  name : cstr})`) rejects a frame with any other
  column-set row at the elaborator (TUR-E0001) instead of missing a lookup at
  runtime. The row erases at codegen; the newtype's carrier is `:int`, so
  coercions stay free. `frame-typed` / `frame-handle` wrap and unwrap; typed
  delegates (`tframe-nrows`, `tcol-int32-at`, `tcol-utf8-at`, ...) thread the
  row through the existing untyped API. Naming a concrete `#row{...}` at a
  call site requires `-Xdata-literals`; the module's own definitions are
  row-polymorphic and need no flag.

## 0.2.0

### Breaking changes

- `group-by` now takes a `Vec[cstr]` as its `names` argument instead of
  a cons list terminated by `0`. Build the argument with
  `(vec-of "col1" "col2" ...)`.

  Before:

  ```turmeric
  (group-by f (cons "g" 0))
  ```

  After:

  ```turmeric
  (group-by f (vec-of "g"))
  ```

  Internal callers that already have a cons list (e.g. `distinct`'s
  computed-default path inside `frame/filter`) use `__group-by-cons`
  directly to avoid a round-trip conversion.

### Note

`agg`, `arrange-indices`, `distinct`, `reorder`, and other frame APIs
still take cons lists for their key/name arguments. Only `group-by`
migrated in this release. (See cons-in-docs cleanup plan: those rows
are classified Case B or out of scope.)

## 0.1.0

- Initial release.
