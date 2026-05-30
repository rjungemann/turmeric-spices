# tur-tourist

Sinatra/scotty-style HTTP micro-framework for Turmeric, layered on top of
`tur-httpd`.

## Overview

`tur-tourist` provides routing, response helpers, middleware, and static file
serving. It follows the idiom from Haskell's
[scotty](https://hackage.haskell.org/package/scotty): a small DSL for
declaring routes and writing handlers.

It is one of three spices that together form a composable web stack for
Turmeric:

| Spice | Analogue | Depends on |
|---|---|---|
| `tur-template` | ERB / EJS | (none -- pure Turmeric) |
| `tur-httpd` | Mongoose / Civetweb | (none -- POSIX sockets + pthreads) |
| `tur-tourist` | Haskell's scotty | `tur-httpd`, `tur-template` |

The three are deliberately separate so any layer can be used independently.

## Install

```turmeric no-check
:spices {
  "tourist" {:url    "https://github.com/turmeric-lang/turmeric-spices"
             :ref    "tourist-v0.1.0"
             :subdir "spices/tourist"}
}
```

## Quick start

```turmeric
(import tourist/app    :refer [tourist])
(import tourist/dsl    :refer [get! post!])
(import tourist/param  :refer [capture])
(import tourist/helpers :refer [text])

(tourist 3000
  (get! "/hello/:name"
    (fn [req]
      (text (str-concat "Hello, " (ok-val (capture req "name")) "!"))))
  (post! "/echo"
    (fn [req]
      (text (req-body req)))))
```

## Status

Early in development. See
[`docs/tur-tourist-plan.md`](../../../turmeric/docs/tur-tourist-plan.md) in the
turmeric repo for the full roadmap.

## See also

- Source: <https://github.com/turmeric-lang/turmeric-spices/tree/main/spices/tourist>
