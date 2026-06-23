# WebSocket Client Guide

`tur-ws-client` is the client half of RFC 6455. It connects Turmeric programs
to any WebSocket endpoint -- local servers, third-party realtime APIs, browser
dev-tools -- and exchanges text and binary frames over a small synchronous,
blocking API. Plain `ws://` has no native dependencies; `wss://` threads
through the bundled mbedTLS.

This guide covers a quick start, the full API, and three common patterns. For
the terse reference, see the [spice README](../../spices/ws-client/README.md).

## Quick start

```turmeric
(import ws-client/client
  :refer [ws-connect ws-conn-null? ws-last-error
          ws-send ws-recv ws-frame-text ws-text? ws-close ws-free])

(let [c (ws-connect "ws://localhost:9000/feed")]
  (if (ws-conn-null? c)
    (println (ws-last-error))                 ;; connect / handshake failed
    (do
      (ws-send c "hello")                     ;; masked text frame
      (let [f (ws-recv c)]                    ;; blocks for the reply
        (when (ws-text? f)
          (println (ws-frame-text f))))
      (ws-close c)                            ;; closing handshake
      (ws-free c))))                          ;; release fd + memory
```

`ws-connect` parses the URI (`ws://host[:port][/path]`, default port 80; `wss`
defaults to 443), opens the socket, performs the HTTP/1.1 Upgrade handshake,
and verifies the server's `Sec-WebSocket-Accept`. On any failure it returns a
**null handle** -- always test with `ws-conn-null?` and read `ws-last-error`.

## Frames

`ws-recv` blocks until a complete message arrives, reassembling fragmented
messages first. It returns an opaque `WsFrame` whose payload is a **view into
the connection's internal buffer, valid only until the next `ws-recv` or
`ws-free`** -- copy it if you need it longer.

```turmeric
(let [f (ws-recv c)]
  (cond
    (ws-text?    f) (handle-text   (ws-frame-text f))
    (ws-binary?  f) (handle-binary (ws-frame-data f) (ws-frame-len f))
    (ws-pong?    f) (note-pong)
    (ws-closed?  f) (shutdown)
    (ws-timeout? f) (idle-tick)
    (ws-error?   f) (reconnect)))
```

`ws-recv` answers Ping frames with a Pong automatically and then surfaces the
Ping (kind `:ping`); Pong and Close frames are surfaced for you to act on.

Kind integers: `1` text, `2` binary, `8` close, `9` ping, `10` pong,
`-1` timeout, `-2` error -- but prefer the predicates above.

## Receive timeouts

By default `ws-recv` blocks indefinitely. `ws-set-timeout` sets `SO_RCVTIMEO`
so an idle socket yields a `:timeout` frame instead:

```turmeric
(ws-set-timeout c 1000)            ;; 1s; pass 0 to block forever again
(let [f (ws-recv c)]
  (when (not (ws-timeout? f))
    (process f)))
```

## TLS (`wss://`)

`wss://` is identical at the call site -- just change the scheme:

```turmeric
(ws-connect "wss://example.com/socket")
```

Internally the connection wraps the TCP socket in an mbedTLS client context
(certificate verification is disabled in v0, matching `tur-http`). TLS requires
the spice to be built with mbedTLS available: run `tur fetch` once. Without it,
a `wss://` connect returns a null handle and `ws-last-error` says to rebuild
with mbedTLS.

## Pattern: echo test

```turmeric
(let [c (ws-connect "ws://localhost:9000/")]
  (ws-send c "ping")
  (println (ws-frame-text (ws-recv c)))   ;; => "ping"
  (ws-close c)
  (ws-free c))
```

## Pattern: market-data feed

```turmeric
(let [c (ws-connect "wss://feed.example.com/stream")]
  (ws-send c "{\"subscribe\":\"BTC-USD\"}")
  (ws-set-timeout c 30000)                 ;; heartbeat watchdog
  (loop []
    (let [f (ws-recv c)]
      (cond
        (ws-text?    f) (do (on-tick (ws-frame-text f)) (recur))
        (ws-timeout? f) (do (log "feed stalled") (recur))
        :else           (do (ws-close c) (ws-free c))))))
```

## Pattern: chat bot

```turmeric
(let [c (ws-connect "wss://chat.example.com/room/42")]
  (loop []
    (let [f (ws-recv c)]
      (when (ws-text? f)
        (let [msg (ws-frame-text f)]
          (when (mentions-me? msg)
            (ws-send c (reply-to msg)))))    ;; pings auto-ponged by ws-recv
      (when (not (ws-closed? f)) (recur))))
  (ws-free c))
```

## Lifecycle and ownership

- `ws-connect` -> `WsConn`; release with `ws-free` (idempotent on a null
  handle). `ws-free` closes the fd, tears down TLS, and frees memory.
- `ws-close` runs the closing handshake (sends Close, drains the echo) and
  closes the socket; still call `ws-free` afterward to release memory.
- `WsFrame` payloads borrow the connection's buffer -- valid only until the
  next `ws-recv`/`ws-free`. `memcpy` (or copy via `ws-frame-text`) to keep them.

## Non-goals (v0)

No async/reactor integration, no per-message deflate (RFC 7692), no HTTP
`CONNECT` proxy tunnelling, no subprotocol/extension negotiation, and outbound
messages are sent as a single frame (no client-side fragmentation).

## See also

- [tur-httpd](../../spices/httpd/README.md) -- HTTP/1.1 server
- [tur-http](../../spices/http/README.md) -- HTTP/HTTPS client (same optional-TLS pattern)
- [tur-tls](../../spices/tls/README.md) -- TLS layer
- [RFC 6455](https://www.rfc-editor.org/rfc/rfc6455) -- The WebSocket Protocol
