# Changelog

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
