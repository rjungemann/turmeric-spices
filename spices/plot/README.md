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
          :ref    "plot-v0.2.0"
          :subdir "spices/plot"}
}
```

## Quick start

```turmeric
(import plot/core  :refer [plot-write-png])
(import plot/line  :refer [function])
(import plot/decor :refer [axes tick-grid])
(import plot/style :refer [default-line-style default-plot-opts])

(plot-write-png
  (vec-of (tick-grid)
          (axes)
          (function (fn [x :float] :float (* x x))
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

plot-write-png
  vec-of(tick-grid()
         axes()
         function((fn [x :float] :float *(x x))
                  -2.0 2.0 128
                  default-line-style()
                  "x^2"))
  default-plot-opts()
  "quadratic.png"
```

## See also

- [Guide](docs/guides/plot-guide.md)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/plot>
