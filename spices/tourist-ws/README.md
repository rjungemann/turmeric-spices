# tur-tourist-ws

WebSocket routes for the [`tur-tourist`](../tourist) framework. Declare a
WebSocket endpoint with `ws-route!` right alongside your `get!` / `post!`
routes -- no dropping down to raw `httpd` to handle the upgrade yourself
(TOUR-V0).

## Overview

`tourist-ws` is the adapter between [`tourist`](../tourist) and
[`ws-server`](../ws-server). `ws-route!` returns an ordinary tourist route
Item, so REST and WebSocket endpoints live in the same item list and share the
same middleware chain. When a request matches a `ws-route!`, the connection is
upgraded (via `ws-server`'s `ws-upgrade`) and your handler runs with the
upgraded `WsConn` for the lifetime of the session.

## Install

```turmeric no-check
:spices {
  "tourist-ws" {:url    "https://github.com/rjungemann/turmeric-spices"
                :ref    "tourist-ws-v0.1.0"
                :subdir "spices/tourist-ws"}
}
```

Within this workspace it resolves as a sibling automatically; it depends on
`tourist`, `ws-server`, `ws-core`, and `httpd`.

## Quick start

Serve a REST route and a WebSocket echo endpoint from one app:

```turmeric no-check
(import tourist/app      :refer [tourist-conn])
(import tourist/dsl      :refer [get!])
(import tourist/helpers  :refer [text])
(import httpd/server     :refer [server-stop])
(import tourist-ws/route :refer [ws-route!])
(import ws-server/server :refer [ws-server-recv ws-server-send ws-server-close])
(import ws-core/conn     :refer [WsConn])
(import ws-core/frame    :refer [ws-frame-text ws-closed? ws-error?])

(defn ws-echo [ws : WsConn] : void
  (let [f (ws-server-recv ws)]
    (if (or (ws-closed? f) (ws-error? f))
      (ws-server-close ws)
      (do (ws-server-send ws (ws-frame-text f))
          (ws-echo ws)))))

(defn main [] : int
  (let [srv (tourist-conn 8080
              (get! "/api" (fn [ctx] (text "hello from REST")))
              (ws-route! "/ws" ws-echo))]
    ;; ... block until shutdown ...
    (server-stop srv)
    0))
```

## Serve with `tourist-conn`, not `tourist`

`ws-route!` needs the underlying httpd `Conn` to perform the upgrade. Only the
Conn-aware entry point **`tourist-conn`** (added in TOUR-V0) makes the Conn
available to route handlers (via `tourist-request-conn`); the plain `tourist`
entry point runs on httpd's single-arg listener where no `Conn` exists, so a
`ws-route!` served under `tourist` will answer `400 Bad Request`. `tourist-conn`
is otherwise a drop-in for `tourist` -- same item list, same middleware chain,
same dispatch.

## API

| Function | Signature | Purpose |
|---|---|---|
| `ws-route!` | `(cstr (fn [WsConn] void)) -> Item` | Declare a GET route that upgrades to a WebSocket and runs the handler with the upgraded `WsConn`. |

The handler is the `ws-server` server-side API: drive it with `ws-server-recv`,
`ws-server-send` / `ws-server-send-bytes`, and `ws-server-close`, and read
frames via the `ws-core/frame` accessors (`ws-frame-text`, `ws-text?`, ...).

## How the upgrade is hijacked

`ws-route!` does not need a bespoke "stop the pipeline" signal from tourist.
Its generated handler calls `ws-upgrade`, which marks the `Conn` upgraded; when
the handler returns, httpd's worker sees `conn-upgraded?` and skips its normal
serialize / write / close step, so the placeholder `Response` is ignored and
the socket the WebSocket session owns is not double-closed. The route still
flows through tourist's middleware chain like any other route.

> Design note: the plan sketched a `request-hijack` escape hatch in tourist
> core plus an `:subprotocols` option on `ws-route!`. The hijack turned out to
> need no core signal -- the existing `conn-mark-upgraded!` handshake between
> `ws-upgrade` and the httpd worker is sufficient -- so TOUR-V0 adds only the
> additive `tourist-conn` entry point (the default `tourist` is untouched).
> Subprotocol negotiation is deferred to SUB-V0.

## Tests

`tests/fixtures/rest-plus-ws/` is the TOUR-V0.3 coexistence fixture: a single
app with both a `/api` REST route and a `/ws` WebSocket route. Type-check it
with `tur check tests/fixtures/rest-plus-ws/main.tur`.
