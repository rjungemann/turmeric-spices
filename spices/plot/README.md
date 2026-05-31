# tur-plot

2D data visualization for Turmeric: functions, points, intervals, histograms,
contours, legends, and PNG output via plutovg.

## Overview

`tur-plot` is a Tier 1 spice implemented in Turmeric with thin inline-C helpers
for sampling and rendering. It draws into an off-screen plutovg surface and
returns that surface handle to the caller, or writes directly to PNG.

It is designed to pair naturally with `tur-plutovg`: use `plot` when you want a
finished chart surface, or `plot-into-canvas` when you need to compose several
plots into a larger image.

## Install

```turmeric no-check
:spices {
  "plot" {:url    "https://github.com/rjungemann/turmeric-spices"
          :ref    "plot-v0.3.0"
          :subdir "spices/plot"}
}
```

## Quick start

The sampled-curve renderers take a typed C-callable: define your function
with `defn` rather than passing an untyped `fn` literal.

```turmeric
(import plot/core  :refer [plot-write-png])
(import plot/line  :refer [function])
(import plot/decor :refer [axes tick-grid])
(import plot/style :refer [default-line-style default-plot-opts])

(defn quadratic [x :float] :float (* x x))

(plot-write-png
  (vec-of (tick-grid)
          (axes)
          (function quadratic
                    -2.0 2.0 128
                    (default-line-style)
                    "x^2"))
  (default-plot-opts)
  "quadratic.png")
```

```sweet-exp
#lang sweet-exp
import plot/core  :refer [plot-write-png]
import plot/line  :refer [function]
import plot/decor :refer [axes tick-grid]
import plot/style :refer [default-line-style default-plot-opts]

defn quadratic [x : float] : float
  *(x x)

plot-write-png
  vec-of(tick-grid()
         axes()
         function(quadratic
                  -2.0 2.0 128
                  default-line-style()
                  "x^2"))
  default-plot-opts()
  "quadratic.png"
```

`samples` (128 above) is a baseline; the renderer adaptively bisects
segments in high-curvature regions and around NaN boundaries up to a fixed
depth, so smooth curves stay cheap and sharp turns get extra samples
without forcing every caller to crank the count globally.

## See also

- [Guide](https://github.com/rjungemann/turmeric-spices/blob/main/spices/plot/docs/guides/plot-guide.md)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/plot>
