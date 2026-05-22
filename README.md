# turmeric-spices

Official monorepo of first-party spices for the [Turmeric](https://github.com/rjungemann/turmeric) ecosystem.

**Canonical repo:** https://github.com/rjungemann/turmeric-spices

---

## Spices

| Spice | Description | Tier | C dep |
|-------|-------------|------|-------|
| [`tur-test`](spices/test/) | Testing framework utilities | 1 -- pure Turmeric | -- |
| [`tur-math`](spices/math/) | 2D/3D vector and matrix math | 1 -- pure Turmeric | -- |
| [`tur-sqlite`](spices/sqlite/) | SQLite3 database bindings | 2 -- cmake-dep | sqlite 3.47.2 |
| [`tur-raylib`](spices/raylib/) | Raylib 5.5 graphics and input | 2 -- cmake-dep | raylib 5.5 |
| [`tur-json`](spices/json/) | JSON parsing and serialization | 3 -- cmake-dep | yyjson 0.10.0 |
| [`tur-http`](spices/http/) | HTTP/HTTPS client | 3 -- cmake-dep | mbedTLS 3.6.2 |
| [`tur-regex`](spices/regex/) | PCRE2 regex bindings | 3 -- cmake-dep | PCRE2 10.44 |

---

## Quick Start

All spices live in a single monorepo. Reference any of them with a `:subdir`
key in your `build.tur`:

```turmeric
:spices {
  "test"   {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "test-v0.1.0"
            :subdir "spices/test"
            :optional true}
  "math"   {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "math-v0.1.0"
            :subdir "spices/math"}
  "sqlite" {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "sqlite-v0.1.0"
            :subdir "spices/sqlite"}
}
```

Or use `tur add` from the command line:

```sh
tur add https://github.com/rjungemann/turmeric-spices \
  --ref test-v0.1.0 --subdir spices/test --name test
```

---

## Spice Reference

### tur-test -- testing utilities

```turmeric
(import test :refer [deftest assert-eq assert-err run-tests])

(deftest "adds correctly"
  (assert-eq (+ 1 2) 3))

(run-tests)
```

Add to your project:

```sh
tur add https://github.com/rjungemann/turmeric-spices \
  --ref test-v0.1.0 --subdir spices/test --name test
```

---

### tur-math -- 2D/3D vector and matrix math

Exports: `math/vec2`, `math/vec3`, `math/mat4`

```turmeric
(import math/vec3 :refer [vec3 dot cross normalize])

(let [a (vec3 1.0 0.0 0.0)
      b (vec3 0.0 1.0 0.0)]
  (println (cross a b)))  ; => (0.0 0.0 1.0)
```

Add to your project:

```sh
tur add https://github.com/rjungemann/turmeric-spices \
  --ref math-v0.1.0 --subdir spices/math --name math
```

---

### tur-sqlite -- SQLite3 bindings

Exports: `sqlite/db`, `sqlite/stmt`, `sqlite/row`

```turmeric
(import sqlite/db  :refer [db-open db-close db-query])
(import sqlite/row :refer [row-get])

(let [db (ok-val (db-open "app.db"))]
  (db-exec db "CREATE TABLE IF NOT EXISTS kv (k TEXT, v TEXT)")
  (db-exec db "INSERT INTO kv VALUES ('hello', 'world')")
  (let [rows (ok-val (db-query db "SELECT * FROM kv"))]
    (for [row rows]
      (println (row-get row "k") (row-get row "v"))))
  (db-close db))
```

Add to your project:

```sh
tur add https://github.com/rjungemann/turmeric-spices \
  --ref sqlite-v0.1.0 --subdir spices/sqlite --name sqlite
```

---

### tur-raylib -- Raylib 5.5 graphics and input

Exports: `raylib/core`, `raylib/shapes`, `raylib/textures`, `raylib/text`,
         `raylib/models`, `raylib/camera`, `raylib/audio`, `raylib/color`,
         `raylib/input`

```turmeric
(import raylib/core   :refer [init-window close-window window-should-close
                               begin-drawing end-drawing clear-background
                               set-target-fps])
(import raylib/text   :refer [draw-text])
(import raylib/color  :refer [raywhite black])

(init-window 800 450 "hello")
(set-target-fps 60)
(while (not (window-should-close))
  (begin-drawing)
  (clear-background (raywhite))
  (draw-text "Hello, Turmeric!" 190 200 20 (black))
  (end-drawing))
(close-window)
```

Add to your project:

```sh
tur add https://github.com/rjungemann/turmeric-spices \
  --ref raylib-v0.1.0 --subdir spices/raylib --name raylib
```

---

### tur-json -- JSON parsing and serialization

Exports: `json/parse`, `json/emit`, `json/patch`

```turmeric
(import json/parse :refer [json-parse json-free])
(import json/emit  :refer [json-emit json-emit-pretty])
(import json/patch :refer [json-get-in json-set-in])

(let [r (json-parse "{\"user\":{\"name\":\"Alice\",\"age\":30}}")]
  (when (ok? r)
    (let [doc (ok-val r)]
      (println (json-get-in doc "user.name"))  ; "Alice"
      (println (json-emit-pretty doc))
      (json-free doc))))
```

Add to your project:

```sh
tur add https://github.com/rjungemann/turmeric-spices \
  --ref json-v0.1.0 --subdir spices/json --name json
```

---

### tur-http -- HTTP/HTTPS client

Exports: `http/client`, `http/request`, `http/response`, `http/error`

```turmeric
(import http/client   :refer [http-get http-post])
(import http/response :refer [response-status response-body])

(let [r (http-get "https://httpbin.org/get")]
  (when (ok? r)
    (let [resp (ok-val r)]
      (println (response-status resp))
      (println (response-body resp)))))
```

Add to your project:

```sh
tur add https://github.com/rjungemann/turmeric-spices \
  --ref http-v0.1.0 --subdir spices/http --name http
```

---

### tur-regex -- PCRE2 regex bindings

Exports: `regex/regex`, `regex/capture`, `regex/error`

```turmeric
(import regex/regex   :refer [regex-compile regex-match regex-replace])
(import regex/capture :refer [capture-at capture-named])

(let [r (regex-compile "(?P<year>\\d{4})-(?P<month>\\d{2})" 0)]
  (when (ok? r)
    (let [re (ok-val r)
          m  (regex-match re "Today is 2026-05-21")]
      (when (ok? m)
        (println (capture-named (ok-val m) "year"))   ; 2026
        (println (capture-named (ok-val m) "month"))) ; 05
      (regex-free re))))
```

Add to your project:

```sh
tur add https://github.com/rjungemann/turmeric-spices \
  --ref regex-v0.1.0 --subdir spices/regex --name regex
```

---

## Tag convention

Each spice is tagged independently:

```
test-v0.1.0
math-v0.1.0
sqlite-v0.1.0
raylib-v0.1.0
json-v0.1.0
http-v0.1.0
regex-v0.1.0
```

Always pin to a tag in production. Omitting `:ref` resolves to HEAD and
prints a warning.

---

## Development

This repo is a Turmeric workspace. To build all spices:

```sh
just build
```

To run tests for a spice:

```sh
cd spices/test && tur test
cd spices/math && tur test
```

C/CMake deps (Tiers 2 and 3) are fetched via `tur fetch` and built with CMake
at compile time. No manual CMake invocation is needed.
