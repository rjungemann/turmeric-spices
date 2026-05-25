# tur-raylib

Raylib 5.5 graphics and input bindings for Turmeric. Windows, shapes, text,
textures, audio, camera, and input -- the full small-game stack.

## Overview

`tur-raylib` is a Tier 2 spice (`cmake-dep` -- pulls in `raylib 5.5` via
`tur fetch`). It exposes the common raylib surface: window management,
`begin-drawing` / `end-drawing` frames, 2D shape and text primitives,
textures, 3D camera helpers, audio playback, and keyboard / gamepad input.

Use it for prototypes, jam games, demos, and learning-oriented graphics
code where raylib's "batteries-included" model is a better fit than raw GL.

## Install

```turmeric no-check
:spices {
  "raylib" {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "raylib-v0.1.0"
            :subdir "spices/raylib"}
}
```

## Quick start

```turmeric
(import raylib/core  :refer [init-window close-window window-should-close
                             begin-drawing end-drawing clear-background
                             set-target-fps])
(import raylib/text  :refer [draw-text])
(import raylib/color :refer [raywhite black])

(init-window 800 450 "hello")
(set-target-fps 60)
(while (not (window-should-close))
  (begin-drawing)
  (clear-background (raywhite))
  (draw-text "Hello, Turmeric!" 190 200 20 (black))
  (end-drawing))
(close-window)
```

```sweet-exp
#lang sweet-exp
import raylib/core  :refer [init-window close-window window-should-close
                             begin-drawing end-drawing clear-background
                             set-target-fps]
import raylib/text  :refer [draw-text]
import raylib/color :refer [raywhite black]

init-window(800 450 "hello")
set-target-fps(60)
while not(window-should-close())
  begin-drawing()
  clear-background(raywhite())
  draw-text("Hello, Turmeric!" 190 200 20 black())
  end-drawing()
close-window()
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/raylib>
