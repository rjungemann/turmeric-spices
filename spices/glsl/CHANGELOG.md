# Changelog

## 0.3.0

### Breaking changes

- Four `glsl/builtins` functions were renamed because their bare names now
  collide with auto-loaded stdlib definitions/macros, which the compiler
  rejects (or which shadow the builtin at call sites and fail to expand):

  | Old      | New          | Collides with         |
  |----------|--------------|-----------------------|
  | `length` | `glsl-length`| stdlib `tur/list` defn `length` |
  | `dot`    | `glsl-dot`   | stdlib `dot` macro    |
  | `min`    | `glsl-min`   | stdlib `min` macro    |
  | `max`    | `glsl-max`   | stdlib `max` macro    |

  The emitted GLSL is unchanged (`glsl-dot` still produces `dot(a, b)`, etc.);
  only the Turmeric-side function names changed.

### Added

- `glsl/ir`: a Fix-encoded IR for glsl, with one generic catamorphism
  (`node-cata`) that every fold is expressed through. The pretty-printer
  (`node->c`) is a single `cstr`-carrier F-algebra whose every clause delegates
  to the matching flat builder (`glsl-binop`, `glsl-if`, `swizzle`, ...), so
  output is byte-identical. A second `int`-carrier fold (`node-size`) shares the
  driver. Smart constructors `ge-*` (expressions, including the glsl-specific
  `ge-swizzle`) and `gs-*` (statements) build `GNode` trees. This is Target 2 of
  the U5 "Fix-encoded IR uplift" plan, ported from the c-dsl prototype. The flat
  builders are unchanged and remain the public surface.

### Fixed

- `glsl/codegen` and `glsl/shaders` test suites now compile and run against
  current stdlib (previously red: the `length`/`dot` collisions above, plus a
  `glsl-sprintf4` test that under-applied the function by one argument). The
  `describe`/`it` suites were also moved inside `main` so their assertions
  actually execute.

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
