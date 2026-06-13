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

```turmeric no-check
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

## Typed encoding (`json/encode`)

The `Encode` typeclass + `derive-json` macro give defstruct types a
JSON serializer without hand-writing per-type code. Primitive instances
ship for `int`, `bool`, and `cstr`; the macro emits one for any
`defstruct` product type whose fields you list explicitly:

```turmeric
(import json/encode :refer [derive-json])

(defstruct User [id : int  name : cstr  active : bool])
(derive-json User (id int) (name cstr) (active bool))

(println (encode (make-struct User 7 "alice" true)))
;; => {"id":7,"name":"alice","active":true}
```

This is the **P2a minimal slice** of the spices type-features uplift
plan (`docs/upcoming/spices-type-features-uplift-plan.md` in the
turmeric repo). It deliberately does not yet ship:

- `Decode` (read-side roundtrip).
- `derive-json` for `defdata` sum types.
- `:as :carrier` opt-in for `defopaque` wire form.
- `:rename-fields` / `:only` / `:skip` codec options.

The earlier 2-field cap (from a closure-codegen bug in the main
turmeric compiler) was lifted 2026-06-12 in the same session that
introduced this module; arbitrary field counts work today.

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/json>
