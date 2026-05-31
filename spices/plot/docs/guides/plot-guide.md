# tur-plot guide

## 1. Basic function plot

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
  "function.png")
```

The `samples` argument (128 above) is a baseline; `function`, `parametric`,
`polar`, and `inverse` feed it to an adaptive subdivision pass that bisects
segments (up to depth 6) wherever neighboring screen-space pieces turn sharply
or cross a NaN/non-NaN boundary. Smooth curves keep the baseline cost; sharp
turns and asymptotes get extra samples without forcing every caller to
crank `samples` globally. `lines` is unaffected -- it plots its caller's
points verbatim and only breaks on NaN coordinates.

## 2. Scatter plot with error bars

Use `points` and `error-bars` together in the same renderer list.

## 3. Shaded interval

Use `function-interval` to fill between two functions.

## 4. Histogram

Use `discrete-histogram` for categorical bars and `stacked-histogram` for
stacked series.

## 5. Contour plot

Use `contours`, `contour-intervals`, and `color-field` together for sampled
scalar fields.

## 6. Multiple renderers and legend

Pass labels on data renderers and set `legend-pos` in `plot-opts`.
