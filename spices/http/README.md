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

```turmeric
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

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/http>
