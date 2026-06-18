# tur-valkey

Valkey/Redis client for Turmeric via hiredis. Connect, run commands, walk
reply trees, and subscribe to pub/sub channels.

## Overview

`tur-valkey` is a `cmake-dep` spice that wraps hiredis. It speaks the
Valkey/Redis RESP protocol, so it works against both Valkey and any
Redis-compatible server. The surface is intentionally small: a `client`
handle, a generic `cmd` plus typed helpers (`cmd-get`, `cmd-set`,
`cmd-incr`, hash and list commands), a recursive `reply` accessor, and a
`pubsub` module for subscribe/publish workflows.

Use it for caching, rate limiting, ephemeral state, and pub/sub messaging
in Turmeric services.

## Install

```turmeric no-check
:spices {
  "valkey" {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "valkey-v0.1.0"
            :subdir "spices/valkey"}
}
```

## Quick start

```turmeric
(import valkey/client :refer [client-connect client-close])
(import valkey/cmd    :refer [cmd-set cmd-get])
(import valkey/reply  :refer [reply-string reply-free])

(let [c (ok-val (client-connect "127.0.0.1" 6379))]
  (cmd-set c "greeting" "hello")
  (let [r (cmd-get c "greeting")]
    (println (reply-string r))
    (reply-free r))
  (client-close c))
```

```sweet-exp
#lang sweet-exp
import valkey/client :refer [client-connect client-close]
import valkey/cmd    :refer [cmd-set cmd-get]
import valkey/reply  :refer [reply-string reply-free]

let [c ok-val(client-connect("127.0.0.1" 6379))]
  cmd-set(c "greeting" "hello")
  let [r cmd-get(c "greeting")]
    println $ reply-string r
    reply-free(r)
  client-close(c)
```

### Linear `Client` (U1)

`Client` is a `:linear` opaque. A connection extracted with `client-of`
(or `ok-val`) must be released exactly once with `client-close`
(`redisFree`); the command, pubsub, and ping operations take it by
`^borrow`, observing the connection without discharging that obligation.
Under `-Xsubstructural` this makes use-after-close and connection leaks
compile-time errors (`TUR-E0101` / `TUR-E0100`) instead of runtime faults.
The discipline is inert in ordinary builds, so existing call sites compile
unchanged. See `tests/errors/` for the rejected cases.

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/valkey>
