# tur-json

JSON parsing and serialization for Turmeric via yyjson: parse, emit, and
in-place patch (`get-in` / `set-in` / `merge`).

## Overview

`tur-json` is a Tier 3 spice (`cmake-dep` -- pulls in `yyjson 0.10.0` via
`tur fetch`). It wraps yyjson's fast SAX/DOM parser behind a small Turmeric
surface: `json-parse` returns a `result<doc>`, `json-emit` /
`json-emit-pretty` serialize back to text, and the `json/patch` module
exposes dotted-path read/write helpers.

Use it for application config, REST payloads, and any JSON I/O where you
want yyjson's speed without dropping to inline C.

## Install

```turmeric
:spices {
  "json" {:url    "https://github.com/rjungemann/turmeric-spices"
          :ref    "json-v0.1.0"
          :subdir "spices/json"}
}
```

## Quick start

```turmeric
(import json/parse :refer [json-parse json-free])
(import json/emit  :refer [json-emit-pretty])
(import json/patch :refer [json-get-in])

(let [r (json-parse "{\"user\":{\"name\":\"Alice\",\"age\":30}}")]
  (when (ok? r)
    (let [doc (ok-val r)]
      (println (json-get-in doc "user.name"))
      (println (json-emit-pretty doc))
      (json-free doc))))
```

```sweet-exp
#lang sweet-exp
import json/parse :refer [json-parse json-free]
import json/emit  :refer [json-emit-pretty]
import json/patch :refer [json-get-in]

let [r json-parse("{\"user\":{\"name\":\"Alice\",\"age\":30}}")]
  when ok?(r)
    let [doc ok-val(r)]
      println $ json-get-in doc "user.name"
      println $ json-emit-pretty doc
      json-free(doc)
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/json>
