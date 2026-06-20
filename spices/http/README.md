# tur-http

HTTP/HTTPS client for Turmeric via mbedTLS: `http-get`, `http-post`, request
and response objects with status and body accessors.

## Overview

`tur-http` is a Tier 3 spice (`cmake-dep` -- pulls in `mbedTLS 3.6.2` via
`tur fetch`). It exposes a small synchronous HTTP/HTTPS client: build a
request, send it, and inspect status / headers / body on the response.

TLS is provided by the bundled mbedTLS so the spice does not depend on
system OpenSSL. Use it for REST API calls, health checks, and any client-side
HTTP integration that does not need full async or HTTP/2.

## Install

```turmeric no-check
:spices {
  "http" {:url    "https://github.com/rjungemann/turmeric-spices"
          :ref    "http-v0.1.0"
          :subdir "spices/http"}
}
```

## Quick start

```turmeric
(import http/client   :refer [http-get])
(import http/response :refer [response-status response-body])

(let [r (http-get "https://httpbin.org/get")]
  (when (ok? r)
    (let [resp (ok-val r)]
      (println (response-status resp))
      (println (response-body resp)))))
```

```sweet-exp
#lang sweet-exp
import http/client   :refer [http-get]
import http/response :refer [response-status response-body]

let [r http-get("https://httpbin.org/get")]
  when ok?(r)
    let [resp ok-val(r)]
      println $ response-status resp
      println $ response-body resp
```

## Typed JSON bodies

`http/request` and `http/response` expose typed JSON codecs built on the json
spice's `Encode`/`Decode` typeclasses, so a request/response body is a typed
struct rather than a hand-walked document:

- `(json-request method url x headers) : int` -- encode `x` as the request
  body and prepend `Content-Type: application/json` to `headers`. Generic over
  any `Encode` instance.
- `(response-decode resp T) : (Result T cstr)` -- decode the response body
  into `T` via its `Decode` instance, or `err` on a non-JSON body. The typed
  counterpart to `response-json` (which returns an untyped doc handle).

```turmeric
(import http/client   :refer [http-request])
(import http/request  :refer [json-request])
(import http/response :refer [response-decode])

(defstruct User [id : int  name : cstr])
(derive-json User (id int) (name cstr))

(let [req (json-request "POST" "https://api.example.com/users"
                        (make-struct User 7 "ann") 0)
      r   (http-request req)]
  (when (ok? r)
    (let [u (response-decode (ok-val r) User)]
      (when (ok? u) (println (.name (ok-val u)))))))
```

These build on json's codec surface, so `http` declares `yyjson` in its
`build.tur` `:cmake-deps` (a workspace-sibling's native deps are not
propagated -- the same pattern `httpd` and `ecs-raylib` use). yyjson, which
was previously optional (only `response-json` used it, stubbing out when
absent), is now a first-class dependency of the typed codec path.

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/http>
