# Session Guide (tur-tourist-session)

This guide shows how to add server-side sessions to a
[`tur-tourist`](../../spices/tourist) app with
[`tur-tourist-session`](../../spices/tourist-session): a cookie + a pluggable
store, modelled on Rack's session middleware.

## The shape of it

A session gives each browser a stable, server-side bag of strings keyed by an
unguessable ID carried in a cookie. The middleware:

1. reads the session cookie on requests that touch the session,
2. loads the data from a **store** (memory, file, or your own),
3. lets handlers read and write it with `session-get` / `session-set!`,
4. saves changes and re-issues the cookie on the way out.

You wire it up with a single item in your `tourist` list and never mention the
store again in handler code — swapping memory for files (or Valkey later) is a
one-line change.

## Hello, session

```turmeric
(import tourist/app          :refer [tourist])
(import tourist/dsl          :refer [get! post!])
(import tourist/helpers      :refer [text redirect])
(import tourist/types        :refer [Ctx])
(import httpd/types          :refer [Response])
(import httpd/server         :refer [server-stop])
(import session/mw           :refer [session-mw])
(import session/config       :refer [dev-session-config])
(import session/memory-store :refer [memory-store-new])
(import session/ctx          :refer [session-get session-set! session-destroy!])

(defn dashboard [ctx : Ctx] : Response
  (let [uid (session-get ctx "user_id")]
    (if (.is-ok uid)
      (text (str-concat "Hello, user " (.ok-val uid)))
      (redirect "/login"))))

(defn do-login [ctx : Ctx] : Response
  (do (session-set! ctx "user_id" "42")
      (redirect "/dashboard")))

(defn do-logout [ctx : Ctx] : Response
  (do (session-destroy! ctx)
      (redirect "/login")))

(defn main [] : int
  (let [store (memory-store-new)
        srv   (tourist 3000
                (session-mw store (dev-session-config))
                (post! "/login"     do-login)
                (get!  "/dashboard" dashboard)
                (post! "/logout"    do-logout))]
    (server-stop srv)
    0))
```

> **Note** `session-mw` goes **directly** in the `tourist` list, not wrapped in
> `use!`. It registers an after-middleware internally and loads the session
> lazily, so it is the only item tourist needs to see.

## Reading and writing

| Call | Returns | Effect |
|------|---------|--------|
| `(session-get ctx "k")` | `(Result cstr cstr)` | `ok` value or `err "missing"` |
| `(session-set! ctx "k" "v")` | — | writes `v`, marks the session dirty |
| `(session-del! ctx "k")` | — | removes `k`, marks dirty |
| `(session-destroy! ctx)` | — | deletes from the store + clears the cookie |
| `(session-id ctx)` | `cstr` | the 64-char hex session ID |

Read a value with field access on the result:

```turmeric
(let [r (session-get ctx "cart_id")]
  (if (.is-ok r)
    (use-cart (.ok-val r))
    (start-new-cart ctx)))
```

The value returned by `session-get` is borrowed from the session and is freed
when the response is flushed — copy it if you need it past the request.

## Choosing a store

```turmeric
;; in-process, fast, lost on restart — great for dev and single-process apps
(session-mw (memory-store-new) (dev-session-config))

;; one JSON file per session under a directory — survives restarts
(session-mw (file-store-new "/var/lib/myapp/sessions") (default-session-config))
```

Nothing in your handlers changes. Both stores sit behind the same `Store`
vtable, and the middleware clones session maps in and out so concurrent
requests never share a mutable map.

### Writing your own store

A store is three functions plus an opaque state pointer:

```turmeric
;; load   : (c-fn [state:int id:cstr] int)            -> SessMap handle, 0 = miss
;; save   : (c-fn [state:int id:cstr map:int ttl:int] int)
;; delete : (c-fn [state:int id:cstr] int)
(store-new my-load my-save my-delete my-state)
```

Build and read the session map with the `session/data` helpers
(`sessmap-new`, `sessmap-set!`, `sessmap-get`, `sessmap-key-at` /
`sessmap-val-at` + `sessmap-count` for iteration). A database- or
Valkey-backed store is just another implementation of these three functions.

## Configuration

`(default-session-config)` is the production preset; `(dev-session-config)`
turns `Secure` off so cookies flow over plain `http://` during development.
Build a custom one with `make-struct`:

```turmeric
(make-struct SessionConfig
  "myapp_sess"   ;; cookie-name
  "/"            ;; path
  ""             ;; domain ("" omits the attribute)
  3600           ;; max-age (seconds)
  1              ;; secure?
  1              ;; http-only?
  "Strict"       ;; same-site
  1)             ;; rolling? (slide the expiry window on each request)
```

`rolling?` re-saves and re-issues the cookie on every request against an
established session so its expiry window slides forward; it is off by default
to avoid extra writes.

## When does it save?

The middleware only does work that is needed:

- **Wrote something** → save + `Set-Cookie`.
- **Called `session-destroy!`** → delete + an expiring `Set-Cookie`.
- **`rolling?` on, established session** → save + `Set-Cookie` (slide expiry).
- **Only read, or never touched** → nothing. Read-only and session-free
  requests get no cookie.

## Security checklist

- IDs are 256-bit CSPRNG values (`getentropy` → `/dev/urandom`), hex-encoded.
  They are bearer tokens — serve over HTTPS in production and keep
  `secure? = 1`.
- This version does **not** rotate the session ID on login, so guard against
  session fixation by calling `session-destroy!` and re-creating the session
  after any privilege change.
- The file store needs a POSIX filesystem (it relies on atomic `rename`).

## Testing

The spice's own suite shows how to exercise sessions without a socket — it
fabricates tourist contexts and drives the real lazy loader and flush:

```sh
tur test spices/tourist-session/tests/session
```
