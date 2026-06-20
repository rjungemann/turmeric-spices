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

## Typed handlers (`Handler` typeclass)

`server-start` takes a bare `(c-fn [int] int)` whose `int`s are really a
`Request` and a `Response` with the types erased. The `httpd/handler` module
lifts that contract to the type layer: a handler is a witness type with a
`Handler` instance whose `respond` method maps a `Request` to a `Response`
directly. `serve` generates the captureless trampoline and starts the server.

```turmeric
(import httpd/handler  :refer [serve])
(import httpd/request  :refer [req-path])
(import httpd/response :refer [resp-ok not-found])
(import httpd/server   :refer [server-stop])

;; The witness is a defopaque :int used only to select the instance; its
;; value carries no state (the method reads everything from the Request).
(defopaque App :int)

(definstance Handler [App]
  (respond [self req]
    (if (cstr-eq? "/health" (req-path (:: req :Request)))
      (resp-ok "text/plain" "ok")
      (not-found "no such route"))))

(def srv (serve 8080 App))     ;; serve-pool / serve-spawn mirror server-start-*
;; ... application code ...
(server-stop srv)
```

The method is named `respond` (not `handle`, which is a reserved special
form). `server-start` and the bare `(c-fn [int] int)` API remain available as
the lower-level path. JSON request/response codec helpers built on the json
spice's `Encode`/`Decode` are a planned follow-up.

## Status

Early in development. See
[`docs/tur-httpd-plan.md`](../../../turmeric/docs/tur-httpd-plan.md) in the
turmeric repo for the full roadmap.

## See also

- [API reference](api/) (generated)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/httpd>
