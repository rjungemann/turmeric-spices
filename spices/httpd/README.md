# tur-httpd

Minimal threaded HTTP/1.1 server for Turmeric, built on POSIX sockets and
`pthreads`. Server-side counterpart to [`tur-http`](../http/).

## Overview

`tur-httpd` is a Tier 1 spice (no external `cmake-dep`). It exposes a small
synchronous request/response API: start a server with a handler function,
inspect the request, build a response, return.

It is one of three spices that together form a composable web stack for
Turmeric:

| Spice          | Analogue            | Depends on                       |
|----------------|---------------------|----------------------------------|
| `tur-template` | ERB / EJS           | (none -- pure Turmeric)          |
| `tur-httpd`    | Mongoose / Civetweb | (none -- POSIX sockets/pthreads) |
| `tur-turist`   | Haskell's scotty    | `tur-httpd`, `tur-template`      |

The three are deliberately separate so any layer can be used independently.

## Install

```turmeric no-check
:spices {
  "httpd" {:url    "https://github.com/rjungemann/turmeric-spices"
           :ref    "httpd-v0.1.0"
           :subdir "spices/httpd"}
}
```

## Quick start

```turmeric
(import httpd/server   :refer [server-start server-stop])
(import httpd/response :refer [resp-ok])

(def srv (server-start 8080 (fn [req]
  (resp-ok "text/plain" "Hello, world!"))))

;; ... application code ...

(server-stop srv)
```

## Status

Early in development. See
[`docs/tur-httpd-plan.md`](../../../turmeric/docs/tur-httpd-plan.md) in the
turmeric repo for the full roadmap.

## See also

- [API reference](api/) (generated)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/httpd>
