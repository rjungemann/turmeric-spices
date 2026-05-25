# tur-png

PNG image read and write for Turmeric via libpng. Decode an existing PNG
into a pixel buffer, manipulate per-pixel RGBA, and write the result back
out.

## Overview

`tur-png` is a `cmake-dep` spice that wraps libpng. It exposes `png-read` /
`png-write` for whole-file I/O, plus an `img` accessor module that gives
you width, height, channel count, bit depth, and per-pixel R/G/B/A
read/write.

It is intentionally minimal -- exactly enough to do generative-art-style
work or post-process the output of `tur-plutovg`. For drawing primitives
use `tur-plutovg`; `tur-png` is the I/O layer.

## Install

```turmeric no-check
:spices {
  "png" {:url    "https://github.com/rjungemann/turmeric-spices"
         :ref    "png-v0.1.0"
         :subdir "spices/png"}
}
```

## Quick start

```turmeric
(import png/reader :refer [png-read img-free])
(import png/info   :refer [img-width img-height pixel-r])

(let [r (png-read "input.png")]
  (when (ok? r)
    (let [img (ok-val r)]
      (println (img-width img) (img-height img))
      (println (pixel-r img 0 0))
      (img-free img))))
```

```sweet-exp
#lang sweet-exp
import png/reader :refer [png-read img-free]
import png/info   :refer [img-width img-height pixel-r]

let [r png-read("input.png")]
  when ok?(r)
    let [img ok-val(r)]
      println(img-width(img) img-height(img))
      println $ pixel-r img 0 0
      img-free(img)
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/png>
