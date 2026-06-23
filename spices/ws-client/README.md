# tur-ws-client

RFC 6455 WebSocket **client** for Turmeric: connect to any `ws://` or `wss://`
endpoint and exchange text and binary frames over a small synchronous API --
`ws-connect`, `ws-send`, `ws-recv`, `ws-close`.

## Overview

`tur-ws-client` implements the client half of RFC 6455. Plain `ws://`
connections go over raw POSIX sockets and have **no native dependencies** --
the handshake's SHA-1 and base64 are implemented inline. `wss://` is layered on
the bundled **mbedTLS 3.6.2** (pulled in via `tur fetch`) and is gated at
compile time on `__has_include(<mbedtls/ssl.h>)`: if mbedTLS is not on the
include path the spice still builds and `ws://` works, while a `wss://` connect
returns a null handle whose `ws-last-error` explains how to enable TLS. This is
the same optional-TLS pattern `tur-http` uses.

It is a blocking, single-threaded client -- ideal for chat bots, market-data
feeds, dev-tools bridges, and echo tests. See *Non-goals* below for what it
deliberately leaves out.

## Install

```turmeric no-check
:spices {
  "ws-client" {:url    "https://github.com/rjungemann/turmeric-spices"
               :ref    "ws-client-v0.1.0"
               :subdir "spices/ws-client"}
}
```

Within this workspace it resolves as a sibling automatically; no fetch needed
for `ws://`. For `wss://`, run `tur fetch` once to build mbedTLS.

## Quick start

```turmeric
(import ws-client/client
  :refer [ws-connect ws-conn-null? ws-last-error
          ws-send ws-recv ws-frame-text ws-text? ws-close ws-free])

(let [c (ws-connect "ws://localhost:9000/feed")]
  (if (ws-conn-null? c)
    (println (ws-last-error))
    (do
      (ws-send c "hello")
      (let [f (ws-recv c)]
        (when (ws-text? f)
          (println (ws-frame-text f))))   ;; payload valid until next ws-recv
      (ws-close c)
      (ws-free c))))
```

## API

| Function | Signature | Purpose |
|---|---|---|
| `ws-connect` | `(cstr) -> WsConn` | Open a `ws://`/`wss://` connection. Null handle on failure. |
| `ws-conn-null?` | `(WsConn) -> bool` | Test the failure sentinel from `ws-connect`. |
| `ws-last-error` | `() -> cstr` | Reason for the most recent failure (static storage). |
| `ws-send` | `(WsConn cstr) -> int` | Send a UTF-8 text frame (masked). |
| `ws-send-bytes` | `(WsConn ptr<void> int) -> int` | Send a binary frame (masked). |
| `ws-recv` | `(WsConn) -> WsFrame` | Receive the next (reassembled) message. |
| `ws-close` | `(WsConn) -> void` | Closing handshake, then close the socket. |
| `ws-free` | `(WsConn) -> void` | Release all resources (fd + memory + TLS). |
| `ws-set-timeout` | `(WsConn int) -> void` | Set receive timeout in ms (0 = block forever). |
| `ws-accept-key` | `(cstr) -> cstr` | Compute `Sec-WebSocket-Accept` for a key (handshake crypto). |

### Reading a frame

`ws-recv` returns an opaque `WsFrame` handle -- a **view into the connection's
internal reassembly buffer**, valid only until the next `ws-recv` or `ws-free`.
Copy the payload if you need it longer.

| Accessor | Returns |
|---|---|
| `ws-frame-kind` | `int` -- `1` text, `2` binary, `8` close, `9` ping, `10` pong, `-1` timeout, `-2` error |
| `ws-frame-data` | `ptr<void>` -- payload pointer |
| `ws-frame-len` | `int` -- payload length |
| `ws-frame-text` | `cstr` -- payload as a NUL-terminated string |

Predicates: `ws-text?`, `ws-binary?`, `ws-ping?`, `ws-pong?`, `ws-closed?`,
`ws-timeout?`, `ws-error?`.

`ws-recv` reassembles fragmented messages, answers Ping frames with a Pong
automatically (then surfaces the Ping), and surfaces Pong and Close frames.

## Receive timeouts

```turmeric
(ws-set-timeout c 500)            ;; 500 ms
(let [f (ws-recv c)]
  (if (ws-timeout? f)
    (println "no message within 500ms")
    (handle f)))
```

`ws-set-timeout` sets `SO_RCVTIMEO`; a fired timeout yields a `:timeout` frame
rather than blocking forever. Pass `0` to disable.

## Non-goals (v0)

No async/reactor integration (blocking I/O only), no per-message deflate
(RFC 7692), no HTTP `CONNECT` proxy tunnelling, no subprotocol negotiation, and
sends are single frames (the client does not fragment outbound messages).

## Tests

- `tests/codec_test.tur` -- flat unit test (run by CI via `tur test tests`):
  asserts `ws-accept-key` against the RFC 6455 §4.2.2 vector, exercising the
  inline SHA-1 + base64 with no network.
- `tests/fixtures/ws-echo-text/run.sh` -- live `ws://` round-trip against a
  pure-Python echo server (text frames + timeout path).
- `tests/fixtures/ws-echo-tls/run.sh` -- live `wss://` round-trip over real TLS
  with a self-signed cert (requires `tur fetch` for mbedTLS).

## See also

- [WebSocket client guide](../../docs/guides/websocket-client-guide.md)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/ws-client>
