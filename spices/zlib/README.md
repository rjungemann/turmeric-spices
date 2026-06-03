# tur-zlib

gzip + raw-deflate encode/decode for Turmeric, wrapping
[zlib](https://github.com/madler/zlib). Binary-safe buffer in /
buffer out, intended as the codec layer for HTTP compression
middleware, log-file post-processing, and anywhere else gzipped or
raw-deflate bytes need to flow through Turmeric.

## Overview

`tur-zlib` is a `cmake-dep` spice that pins upstream zlib via
`:cmake-deps` for reproducible builds. It exposes two encoder/decoder
pairs:

- `gzip-encode` / `gzip-decode` -- standard gzip wrapper (magic
  bytes `1F 8B`). What `Content-Encoding: gzip` over HTTP carries.
- `deflate-raw` / `inflate-raw` -- raw deflate streams, no wrapper.

Every encode/decode returns an opaque **GzipBuf** handle that bundles
the produced bytes with their length in one allocation. Use
`gzip-buf-data` + `gzip-buf-len` to read the payload, then
`gzip-buf-free` to release it.

The GzipBuf shape -- length-prefixed, single allocation -- exists so
that callers can safely move binary output (which may contain embedded
NUL bytes) through pointer + length APIs without ever passing it
through `strlen`.

## Install

```turmeric no-check
:spices {
  "zlib" {:url    "https://github.com/rjungemann/turmeric-spices"
          :ref    "zlib-v0.1.0"
          :subdir "spices/zlib"}
}
```

The spice pins `madler/zlib` at `v1.3.1` and links it statically
(`BUILD_SHARED_LIBS=OFF`). No system zlib is required at build or
runtime.

## Quick start

```turmeric
(import tur/zlib :refer [gzip-encode gzip-decode
                         gzip-buf-data gzip-buf-len gzip-buf-free])

(let [enc (gzip-encode src src-len)]
  (let [dec (gzip-decode (gzip-buf-data enc) (gzip-buf-len enc))]
    ;; (gzip-buf-data dec) ... (gzip-buf-len dec) bytes are the original
    (gzip-buf-free dec))
  (gzip-buf-free enc))
```

```sweet-exp
#lang sweet-exp
import tur/zlib :refer [gzip-encode gzip-decode
                        gzip-buf-data gzip-buf-len gzip-buf-free]

let [enc gzip-encode(src src-len)]
  let [dec gzip-decode(gzip-buf-data(enc) gzip-buf-len(enc))]
    gzip-buf-free(dec)
  gzip-buf-free(enc)
```

## GzipBuf lifecycle

1. `gzip-encode` / `gzip-decode` / `deflate-raw` / `inflate-raw` return
   a fresh GzipBuf (or NULL on failure).
2. Read the payload with `gzip-buf-data` / `gzip-buf-len` -- the
   pointer is valid until you free the buffer.
3. Call `gzip-buf-free` exactly once when done.

The buffer keeps `len + bytes` in a single allocation, so there is no
separate length to track and no double-free hazard between data and
length.

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/zlib>
- Upstream zlib: <https://github.com/madler/zlib>
