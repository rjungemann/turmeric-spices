# Changelog

## 0.2.0

### Breaking changes

- The shader-builder functions
  (`glsl-vertex-shader`, `glsl-fragment-shader`,
  `glsl-compute-shader`, `glsl-geometry-shader`) now take a
  `Vec[cstr]` as their `decls` argument instead of a cons list
  terminated by `0`. Build the argument with `(vec-of ...)`, or pass
  `(vec-of)` for no declarations.

  Before:

  ```turmeric
  (glsl-fragment-shader "330 core"
    (cons (glsl-output "FragColor" ":vec4") 0)
    body)
  ```

  After:

  ```turmeric
  (glsl-fragment-shader "330 core"
    (vec-of (glsl-output "FragColor" ":vec4"))
    body)
  ```

### Added

- `glsl-join-vec`: like `glsl-join` but operates on `Vec[cstr]`.
  Exported from `glsl/codegen`. The old `glsl-join` (cons-list based)
  is kept for internal callers (mat2/mat3/mat4/etc. argument joining).

## 0.1.0

- Initial release.
