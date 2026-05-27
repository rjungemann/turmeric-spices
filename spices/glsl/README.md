# tur-glsl

Lisp-syntax DSL that compiles to GLSL shader source. Author vertex and
fragment shaders in Turmeric and emit GLSL strings ready to feed to
`tur-opengl` (or any other GL backend).

## Overview

`tur-glsl` is a Tier 1 spice (pure Turmeric, no C deps). It mirrors the
shape of `tur-c-dsl` but targets GLSL: shader stages, uniforms, inputs,
outputs, and the standard GLSL built-ins are modeled as Turmeric values
and rendered by `compile-glsl`.

Pair it with `tur-opengl` for an end-to-end "shader in Turmeric" workflow,
or use it stand-alone to bake GLSL strings at build time.

## Install

```turmeric no-check
:spices {
  "glsl" {:url    "https://github.com/rjungemann/turmeric-spices"
          :ref    "glsl-v0.1.0"
          :subdir "spices/glsl"}
}
```

## Quick start

```turmeric
(import glsl/shaders  :refer [glsl-fragment-shader glsl-output])
(import glsl/core     :refer [glsl-set!])
(import glsl/codegen  :refer [compile-glsl])

(println
  (compile-glsl
    (glsl-fragment-shader "330 core"
      (list (glsl-output "FragColor" ":vec4"))
      (glsl-set! "FragColor" "vec4(1.0, 0.5, 0.2, 1.0)"))))
```

```sweet-exp
#lang sweet-exp
import glsl/shaders :refer [glsl-fragment-shader glsl-output]
import glsl/core    :refer [glsl-set!]
import glsl/codegen :refer [compile-glsl]

println $
  compile-glsl
    glsl-fragment-shader "330 core"
      list(glsl-output("FragColor" ":vec4"))
      glsl-set!("FragColor" "vec4(1.0, 0.5, 0.2, 1.0)")
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/glsl>
