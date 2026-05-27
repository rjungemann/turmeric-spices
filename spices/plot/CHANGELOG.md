# Changelog

## 0.2.0

### Breaking changes

- `plot`, `plot-into-canvas`, and `plot-write-png` now take a
  `Vec[Renderer]` as their `renderers` argument instead of a cons list
  terminated by `0`. Build the argument with `(vec-of ...)`, or with
  `vec-new` + `vec-push!` for dynamic construction.

  Before:

  ```turmeric
  (plot-write-png
    (list (tick-grid) (axes)
          (function (fn [x :float] :float (* x x)) -2.0 2.0 128
                    (default-line-style) "x^2"))
    (default-plot-opts)
    "quadratic.png")
  ```

  After:

  ```turmeric
  (plot-write-png
    (vec-of (tick-grid) (axes)
            (function (fn [x :float] :float (* x x)) -2.0 2.0 128
                      (default-line-style) "x^2"))
    (default-plot-opts)
    "quadratic.png")
  ```

  Inner renderer payload data (lines/points/etc. sample lists) is
  unchanged -- still cons-shaped.

  Callers that still pass a cons list will get a runtime crash when the
  C inline-loop dereferences `Vec.data`, not a type error, because both
  types are carried as `:int` at the type-system level.

## 0.1.0

- Initial release.
