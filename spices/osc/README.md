# tur-osc

Open Sound Control (OSC) messaging for Turmeric via liblo. UDP/TCP servers,
clients, messages, and bundles.

## Overview

`tur-osc` is a `cmake-dep` spice that wraps liblo. It exposes the four core
OSC types: servers (UDP or TCP, blocking or non-blocking receive), clients
that send to a destination URL, individual messages with float/int/string
arguments, and bundles that group messages with a single timetag.

Use it to drive SuperCollider (scsynth/hcsynth), control max/MSP or
TouchDesigner patches, or wire up any OSC-speaking instrument or controller.
Pairs naturally with `tur-scscm` for SuperCollider synthesis.

## Install

```turmeric no-check
:spices {
  "osc" {:url    "https://github.com/rjungemann/turmeric-spices"
         :ref    "osc-v0.1.0"
         :subdir "spices/osc"}
}
```

## Quick start

```turmeric
(import osc/client :refer [client-new client-send client-free])
(import osc/msg    :refer [msg-new msg-add-float msg-free])

(let [r (client-new "127.0.0.1" "57110" "udp")]
  (if (ok? r)
    (let [c   (ok-val r)
          msg (msg-new "/s_new")]
      (msg-add-float msg 440.0)
      (client-send c msg)
      (msg-free msg)
      (client-free c))
    (println "client-new failed")))
```

```sweet-exp
#lang sweet-exp
import osc/client :refer [client-new client-send client-free]
import osc/msg    :refer [msg-new msg-add-float msg-free]

let [r client-new("127.0.0.1" "57110" "udp")]
  if ok?(r)
    let [c   ok-val(r)
         msg msg-new("/s_new")]
      msg-add-float(msg 440.0)
      client-send(c msg)
      msg-free(msg)
      client-free(c)
    println("client-new failed")
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/osc>
