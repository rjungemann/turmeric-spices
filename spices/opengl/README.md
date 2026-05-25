# tur-opengl

OpenGL 3.3 Core + GLFW + GLAD bindings for Turmeric. A complete modern-GL
binding with window/context creation, buffer and shader helpers, draw calls,
and small matrix utilities.

## Overview

`tur-opengl` is a Tier 2 spice (`cmake-dep` -- pulls in `glfw 3.4` and
`glad v2.0.6` via `tur fetch`). It exposes the common GL surface: windows,
vertex arrays, buffers, shader programs, textures, draw calls, and input.

Pairs naturally with `tur-glsl` for shader authoring and `tur-math` for the
linear-algebra side. Use it for desktop graphics demos, tools, and
games-of-modest-size that want direct GL access.

## Install

```turmeric
:spices {
  "opengl" {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "opengl-v0.1.0"
            :subdir "spices/opengl"}
}
```

## Quick start

```turmeric
(import opengl/window :refer [make-window destroy-window window-should-close?
                              poll-events swap-buffers set-clear-color clear])

(let [w (make-window 800 600 "Hello")]
  (set-clear-color 0.1 0.1 0.1 1.0)
  (while (not (window-should-close? w))
    (clear)
    (swap-buffers w)
    (poll-events))
  (destroy-window w))
```

```sweet-exp
#lang sweet-exp
import opengl/window :refer [make-window destroy-window window-should-close?
                              poll-events swap-buffers set-clear-color clear]

let [w make-window(800 600 "Hello")]
  set-clear-color(0.1 0.1 0.1 1.0)
  while not(window-should-close?(w))
    clear()
    swap-buffers(w)
    poll-events()
  destroy-window(w)
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/opengl>
