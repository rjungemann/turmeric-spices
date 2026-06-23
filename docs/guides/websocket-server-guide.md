# WebSocket Server Guide

`tur-ws-server` is the server half of RFC 6455. It lifts a connection handled
by [`tur-httpd`](../../spices/httpd) from HTTP/1.1 to a WebSocket session: an
ordinary HTTP request arrives with `Upgrade: websocket`, your handler calls
`ws-upgrade`, and from then on the two sides exchange text and binary frames
over a small synchronous, blocking API. Plain `ws://` has no native
dependencies (the handshake's SHA-1 + base64 are inline).

This guide covers a quick start, how the upgrade plugs into httpd, the full
API, and three common patterns (echo, mixed REST + WS, and a broadcast hub).
For the terse reference, see the [spice README](../../spices/ws-server/README.md).

## Prerequisites: the two-arg handler

httpd's classic handler is `(fn [Request] Response)`. A WebSocket upgrade needs
the raw socket, so ws-server builds on httpd's **two-arg** handler shape, opted
into with `server-start-conn` (or `serve-conn`):

```turmeric
(fn [conn : Conn  req : Request] : Response)
```

The extra `Conn` is an opaque handle over the worker's client fd plus an
"upgraded" flag. `ws-upgrade` reads the fd, writes the `101 Switching
Protocols` response directly, and marks the `Conn` upgraded so the httpd worker
loop skips its normal serialize/write/close step -- ownership of the socket has
transferred to your WebSocket handler.

Existing single-arg httpd servers are unaffected: `server-start`,
`server-start-pool`, `serve`, and friends keep working unchanged.

## Quick start: an echo server

```turmeric
(import httpd/types    :refer [Conn Request])
(import httpd/server   :refer [server-start-conn server-stop])
(import httpd/request  :refer [req-path])
(import httpd/response :refer [resp-ok bad-request])
(import ws-server/server
  :refer [WsConn ws-upgrade ws-server-recv ws-server-send ws-server-close
          ws-frame-text ws-text? ws-binary? ws-closed?])

(defn echo-loop [ws : WsConn] : void
  (let [f (ws-server-recv ws)]               ;; blocks for the next message
    (if (ws-closed? f)
      (ws-server-close ws)                   ;; client closed -> close back
      (do
        (when (or (ws-text? f) (ws-binary? f))
          (ws-server-send ws (ws-frame-text f)))
        (echo-loop ws)))))

(defn handler [conn : Conn  req : Request] : Response
  (if (= (req-path req) "/ws")
    (if (= (ws-upgrade conn req echo-loop) 1)
      (resp-ok "text/plain" "")              ;; placeholder; worker skips it
      (bad-request "not a websocket request"))
    (resp-ok "text/plain" "try /ws")))

(defn main [] : int
  (let [srv (server-start-conn 8080 handler)]
    ;; ... block on a signal, etc. ...
    (server-stop srv)
    0))
```

`ws-upgrade` returns `1` when the request was a valid WebSocket Upgrade and the
handler ran to completion, or `0` when it was not (your handler should then
return a normal HTTP error response). When it returns `1`, the `Response` you
return afterward is **ignored** -- it is only there to satisfy httpd's type.

## How the upgrade works

`ws-upgrade conn req handler`:

1. Reads `Upgrade` (must name `websocket`) and `Sec-WebSocket-Key` from `req`.
   If either is missing/wrong it returns `0` and does nothing else.
2. Computes `Sec-WebSocket-Accept = base64(sha1(key + GUID))` and writes the
   `101 Switching Protocols` response straight to the fd (bypassing
   `serialize-response`, which would add a `Content-Length`).
3. Marks the `Conn` upgraded so the worker loop will not write a second
   response or close the socket.
4. Wraps the fd in a server-side `WsConn` and calls your `handler` with it.
5. Releases the `WsConn` when `handler` returns. If `handler` did not already
   close the socket via `ws-server-close`, it is closed here.

## Frames

`ws-server-recv` blocks until a complete message arrives, reassembling
fragmented messages first. It returns an opaque `WsFrame` whose payload is a
**view into the connection's reassembly buffer, valid only until the next
`ws-server-recv`**. Copy it if you need it longer.

| Accessor | Returns |
|---|---|
| `ws-frame-kind` | `int` -- `1` text, `2` binary, `8` close, `9` ping, `10` pong, `-1` timeout, `-2` error |
| `ws-frame-data` | `ptr<void>` -- payload pointer |
| `ws-frame-len` | `int` -- payload length |
| `ws-frame-text` | `cstr` -- payload as a NUL-terminated string |

Predicates: `ws-text?`, `ws-binary?`, `ws-ping?`, `ws-pong?`, `ws-closed?`,
`ws-timeout?`, `ws-error?`.

Ping frames are answered with a Pong automatically (then surfaced); Pong and
Close frames are surfaced to you. Outbound frames are sent **unmasked**, as
RFC 6455 §5.1 requires for server-to-client frames.

### Receive timeouts

```turmeric
(ws-set-server-timeout ws 30000)             ;; 30s idle timeout
(let [f (ws-server-recv ws)]
  (if (ws-timeout? f)
    (ws-server-close ws)                     ;; idle too long -> drop it
    (handle f)))
```

`ws-set-server-timeout` sets `SO_RCVTIMEO`; a fired timeout yields a `:timeout`
frame instead of blocking forever. Pass `0` to disable. Setting a timeout is a
good idea on the worker thread so a silent client cannot pin a worker forever.

## Pattern: mixed REST + WebSocket on one port

```turmeric
(defn handler [conn : Conn  req : Request] : Response
  (cond
    (and (= (req-method req) "GET") (= (req-path req) "/health"))
    (resp-ok "text/plain" "ok")

    (and (= (req-method req) "GET") (= (req-path req) "/ws"))
    (if (= (ws-upgrade conn req echo-loop) 1)
      (resp-ok "text/plain" "")
      (bad-request "not a websocket request"))

    :else (not-found "")))
```

REST routes return their `Response` as usual; only the `/ws` route upgrades.

## Pattern: a broadcast hub

There is no built-in pub/sub -- a hub is a few lines of user code over a
`Mutex`-guarded collection of `WsConn`s. Each upgraded handler registers its
`WsConn`, then loops; a message from any client is forwarded to all registered
connections. The shape:

```turmeric
;; A shared, mutex-guarded vector of live WsConns (pseudocode shape).
;; register! adds ws under the lock; broadcast! iterates under the lock and
;; calls ws-server-send on each; unregister! removes ws on disconnect.

(defn hub-handler [ws : WsConn] : void
  (do
    (register! HUB ws)
    (let loop []
      (let [f (ws-server-recv ws)]
        (if (ws-closed? f)
          (do (unregister! HUB ws) (ws-server-close ws))
          (do (broadcast! HUB (ws-frame-text f)) (loop)))))))
```

Because each connection is serviced on its own worker thread, the only shared
state is the connection list -- guard it with a `Mutex` and keep the critical
section to the list operations (do the `ws-server-send` calls while holding the
lock so a concurrent `unregister!` cannot free a connection mid-send).

## Limitations (v0)

- **No `wss://`.** httpd has no TLS listener yet; put a TLS-terminating reverse
  proxy in front, or use `ws-client` for outbound `wss://`. The `Conn` reserves
  a `tls` slot for when httpd grows TLS.
- No per-message deflate (RFC 7692), subprotocol negotiation, or HTTP/2.
- Blocking receive only (one worker thread per connection).

## See also

- [WebSocket server README](../../spices/ws-server/README.md)
- [WebSocket client guide](websocket-client-guide.md) -- the client counterpart
- [tur-httpd](../../spices/httpd) -- the HTTP/1.1 server this upgrades from
