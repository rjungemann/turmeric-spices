# tur-plutovg -- 2D Vector Graphics Guide

> Spice version 0.1.0 -- plutovg 1.3.3
> Audience: Turmeric users who want off-screen 2D rendering (PNG/JPEG output, SVG-shape rasterisation, text composition).
> If you need *real-time* drawing prefer `tur-raylib`; `tur-plutovg` is built for static images.

This guide walks through the five things you'll do 95% of the time:

1. [Create a surface and canvas](#1-creating-a-surface-and-canvas)
2. [Draw basic shapes with solid and gradient fills](#2-shapes-and-fills)
3. [Load a font and render text](#3-fonts-and-text)
4. [Export the result to PNG](#4-exporting-the-result)
5. [Pass pixels to `tur-png` for fine-grained edits](#5-integration-with-tur-png)

Each section is a self-contained snippet you can drop into a `defn main`.

---

## 0. Installing the spice

In your project's `build.tur`:

```turmeric
:spices #{
  "plutovg" #{:url    "https://github.com/rjungemann/turmeric-spices"
              :ref    "plutovg-v0.1.0"
              :subdir "spices/plutovg"}
}
```

Then:

```sh
tur fetch        # downloads plutovg, runs cmake, builds libplutovg.a
```

The first fetch takes 15-30 seconds (cmake + 11 plutovg .c files); subsequent ones reuse the cached build.

---

## 1. Creating a surface and canvas

Every render targets a **surface** (a pixel buffer) and goes through a **canvas** (the drawing context).

```turmeric
(import plutovg/surface :refer [surface-create surface-destroy])
(import plutovg/canvas  :refer [canvas-create canvas-destroy])

(let [sr (surface-create 800 600)]
  (when (ok? sr)
    (let [s  (ok-val sr)
          cr (canvas-create s)]
      (when (ok? cr)
        (let [c (ok-val cr)]
          ;; ... draw here ...
          (canvas-destroy c)))
      (surface-destroy s))))
```

Surfaces hold premultiplied ARGB pixels (32 bits per pixel; on a little-endian host the in-memory byte order is `B G R A`). The canvas inherits the surface's dimensions; it never owns the surface, so always destroy in the reverse order of creation.

> **`ok?` and `ok-val`** are auto-loaded stdlib helpers from `stdlib/result.tur`; no explicit `(import result ...)` is needed.

---

## 2. Shapes and fills

### Solid fill

```turmeric
(import plutovg/canvas :refer [canvas-set-source-color
                               canvas-circle canvas-fill
                               canvas-rect canvas-stroke
                               canvas-set-line-width])

(canvas-set-source-color c 0.13 0.55 0.99 1.0)   ; sky blue
(canvas-circle c 400.0 300.0 120.0)
(canvas-fill c)

(canvas-set-source-color c 0.0 0.0 0.0 1.0)      ; black outline
(canvas-set-line-width c 3.0)
(canvas-circle c 400.0 300.0 120.0)
(canvas-stroke c)
```

Two things to note:

- `canvas-fill` and `canvas-stroke` *consume* the recorded path. If you want to fill **and** stroke the same path, build it in a standalone `plutovg/path` and use `canvas-fill-path` / `canvas-stroke-path`, or copy the path with `path-clone`.
- The color components are linear floats in `[0, 1]`. There's also `paint-create-color-hex` if you prefer CSS-style strings -- see below.

### Linear gradient

Gradients need their stops, spread method, and matrix at construction time (plutovg's API has no post-hoc setters), so we build a small **stops buffer** first.

```turmeric
(import plutovg/paint :refer [gradient-stops-create gradient-stops-add
                              gradient-stops-destroy
                              paint-create-linear-gradient
                              paint-destroy canvas-set-source])

(let [sbr   (gradient-stops-create)
      stops (ok-val sbr)]
  (gradient-stops-add stops 0.0  0.13 0.55 0.99 1.0)  ; sky blue
  (gradient-stops-add stops 1.0  0.99 0.40 0.13 1.0)  ; coral
  (let [pr    (paint-create-linear-gradient
                 0.0 0.0  800.0 600.0  ":pad" stops)
        paint (ok-val pr)]
    (canvas-set-source c paint)
    (canvas-rect c 0.0 0.0 800.0 600.0)
    (canvas-fill c)
    (paint-destroy paint))
  (gradient-stops-destroy stops))
```

For a radial gradient swap in `paint-create-radial-gradient` (`cx cy cr fx fy fr spread stops`). The same stops buffer works for both flavours.

The `:pad` spread keyword keeps the gradient's edge colors at the extents; `:reflect` and `:repeat` are the other options.

### Hex colors

```turmeric
(let [pr (paint-create-color-hex "#22aaff")]
  (when (ok? pr)
    (canvas-set-source c (ok-val pr))
    (canvas-rect c 100.0 100.0 200.0 200.0)
    (canvas-fill c)
    (paint-destroy (ok-val pr))))
```

`paint-create-color-hex` understands the syntaxes plutovg's CSS color parser accepts: `#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb(...)`, `rgba(...)`, named colors like `"red"`, etc.

---

## 3. Fonts and text

Text needs a font face. You either load one off disk or set up a face cache keyed by family + style.

### Load a single face

```turmeric
(import plutovg/font :refer [font-face-load-from-file font-face-destroy
                             canvas-set-font canvas-fill-text
                             canvas-text-extents])
(import plutovg/path :refer [rect-w rect-h rect-destroy])

(let [fr (font-face-load-from-file
            "/System/Library/Fonts/Supplemental/Arial.ttf" 0)]
  (when (ok? fr)
    (let [ff (ok-val fr)]
      (canvas-set-font c ff 36.0)
      (canvas-set-source-color c 0.1 0.1 0.1 1.0)
      (canvas-fill-text c "Hello, world!" ":utf8" 50.0 100.0)

      ;; Measure for layout
      (let [er  (canvas-text-extents c "Hello, world!" ":utf8")
            box (ok-val er)]
        (println (rect-w box) "x" (rect-h box))
        (rect-destroy box))

      (font-face-destroy ff))))
```

Coordinates for `canvas-fill-text` are the *baseline* origin, not the top-left of the glyphs. Use `font-face-ascent` / `font-face-descent` if you need to convert from a top-left layout.

### Font cache

For larger applications it's cleaner to register all your fonts up front:

```turmeric
(import plutovg/font :refer [font-cache-create font-cache-destroy
                             font-cache-add-file font-cache-get
                             canvas-set-font])

(let [fc (ok-val (font-cache-create))]
  (font-cache-add-file fc "/fonts/inter/Inter-Regular.ttf")
  (font-cache-add-file fc "/fonts/inter/Inter-Bold.ttf")
  ;; ... draw with whichever style you need
  (let [ff (ok-val (font-cache-get fc "Inter" ":bold" 24.0))]
    (canvas-set-font c ff 24.0)
    (canvas-fill-text c "Headline" ":utf8" 50.0 80.0))
  (font-cache-destroy fc))   ; releases all registered faces
```

`font-cache-get` returns a borrowed face -- do **not** `font-face-destroy` it. Let `font-cache-destroy` clean everything up.

If you want plutovg to discover the platform's standard font directories, call `font-cache-load-system fc` instead of (or in addition to) `add-file`. That works on macOS and Linux; behaviour on other platforms depends on plutovg's build.

---

## 4. Exporting the result

PNG and JPEG are one-liners:

```turmeric
(import plutovg/surface :refer [surface-write-png surface-write-jpeg])

(surface-write-png  s "render.png")
(surface-write-jpeg s "render.jpg" 90)   ; quality in [0, 100]
```

Both return `(ok 0)` / `(err 0)` results, so check before claiming success:

```turmeric
(let [r (surface-write-png s "render.png")]
  (if (ok? r)
    (println "wrote render.png")
    (println "PNG write failed")))
```

---

## 5. Integration with tur-png

`tur-plutovg` rasterises a scene into a tightly packed pixel buffer; `tur-png` lets you read and edit those pixels by hand. The bridge is `surface-data` and `tur-png`'s `png-write-raw`:

```turmeric
(import plutovg/surface :refer [surface-data surface-width surface-height])
(import png/writer      :refer [png-write-raw])

;; ... draw with plutovg into surface `s` ...

(png-write-raw "out.png"
               (surface-data s)
               (surface-width s)
               (surface-height s)
               4    ; channels: RGBA
               8)   ; bit depth
```

Two byte-order caveats:

- plutovg writes pixels as **premultiplied** ARGB: a pixel `(r, g, b, a)` is stored as `(r*a/255, g*a/255, b*a/255, a)`. If you want unmultiplied output (the usual PNG convention), divide each colour channel by `a/255` before handing the buffer to `tur-png`.
- On a little-endian host the in-memory layout is `B G R A`. `tur-png`'s `png-write-raw` accepts that layout when the channel count is 4 -- it stores rows verbatim. If you need RGBA-ordered bytes (for an external API), shuffle in place after rendering.

For most workflows -- "draw with plutovg, save with plutovg" -- skip `tur-png` entirely and call `surface-write-png`. Reach for `tur-png` when you need to *read* an existing PNG into plutovg or post-process individual pixels in Turmeric.

---

## Cleanup checklist

In reverse construction order:

1. `paint-destroy` every paint you created.
2. `path-destroy` every standalone `plutovg/path`.
3. `font-face-destroy` only the faces you loaded directly with `font-face-load-from-file` / `font-face-load-from-data`. Faces obtained from a font cache are owned by the cache.
4. `font-cache-destroy` to release the cache (and every cached face).
5. `gradient-stops-destroy` / `dash-array-destroy` for builder buffers.
6. `rect-destroy` for any rect handles returned by `path-extents` / `font-face-text-extents` / `canvas-text-extents`.
7. `canvas-destroy` the canvas.
8. `surface-destroy` the surface last.

Forgetting any one of these is a memory leak, not a crash -- plutovg uses reference-counted internals and a failed cleanup is recoverable, but valgrind / ASAN will complain.

---

## Where to go next

- `tur-raylib` -- if you need real-time / windowed rendering instead of static images.
- `tur-png` -- pixel-level edits on plutovg's output.
- `plutovg/path` -- the standalone path object covers SVG path-data parsing (`path-parse`), flattening (`path-clone-flatten`), dashing (`path-clone-dashed`), and bounding-box queries (`path-extents`). Useful when you want to compute geometry once and stamp it many times.
- `plutovg/canvas` -- there's a full state stack (`canvas-save` / `canvas-restore`), Porter-Duff blend operators (`canvas-set-operator`), and global opacity (`canvas-set-opacity`) for compositing effects.
