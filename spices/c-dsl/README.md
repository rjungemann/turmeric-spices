# tur-c-dsl

Lisp-syntax DSL that compiles to C99 source code. Build C source trees from
Turmeric data structures and emit ready-to-compile `.c` strings.

## Overview

`tur-c-dsl` is a Tier 1 spice (pure Turmeric, no C deps). It models C
declarations, statements, and expressions as Turmeric values, then renders
them to a formatted C99 source string via `compile-c`.

It is most useful as a code-generation target -- e.g. emitting wrappers,
shader-side data layouts, or compiled DSLs -- where building C source by
string concatenation would be error-prone.

## Install

```turmeric
:spices {
  "c-dsl" {:url    "https://github.com/rjungemann/turmeric-spices"
           :ref    "c-dsl-v0.1.0"
           :subdir "spices/c-dsl"}
}
```

## Quick start

```turmeric
(import c-dsl/codegen :refer [compile-c c-stmts])
(import c-dsl/fns     :refer [c-defn c-param c-call])
(import c-dsl/core    :refer [c-return c-binop])

(let [body (c-stmts (cons (c-return (c-binop "+" "a" "b")) 0))
      fn   (c-defn "add"
                   (cons (c-param "a" ":int")
                         (cons (c-param "b" ":int") 0))
                   ":int" body)]
  (println (compile-c (cons fn 0))))
```

```sweet-exp
#lang sweet-exp
import c-dsl/codegen :refer [compile-c c-stmts]
import c-dsl/fns     :refer [c-defn c-param c-call]
import c-dsl/core    :refer [c-return c-binop]

let [body c-stmts(cons(c-return(c-binop("+" "a" "b")) 0))
     fn   c-defn("add"
                 cons(c-param("a" ":int") cons(c-param("b" ":int") 0))
                 ":int" body)]
  println $ compile-c cons(fn 0)
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/c-dsl>
