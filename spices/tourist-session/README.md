# tur-tourist-session

Swappable-store **session middleware** for the [`tur-tourist`](../tourist)
HTTP micro-framework.

- Reads / writes a session cookie on every request that touches the session
- Loads and saves session data through a **pluggable store** so the storage
  backend swaps without touching application code
- Ships two stores out of the box:
  - **memory store** — in-process, mutex-guarded; no external deps
  - **file store** — one JSON file per session; survives restarts

The API is modelled on Rack's session middleware, adapted to Turmeric's
ownership model and tourist's context pattern.

## Quick start

```turmeric
(import tourist/app          :refer [tourist])
(import tourist/dsl          :refer [get! post!])
(import tourist/helpers      :refer [text redirect])
(import session/mw           :refer [session-mw])
(import session/config       :refer [dev-session-config])
(import session/memory-store :refer [memory-store-new])
(import session/ctx          :refer [session-get session-set! session-destroy!])

(defn dashboard [ctx : Ctx] : Response
  (let [uid (session-get ctx "user_id")]
    (if (.is-ok uid)
      (text (str-concat "Hello, user " (.ok-val uid)))
      (redirect "/login"))))

(defn main [] : int
  (let [store (memory-store-new)
        srv   (tourist 3000
                (session-mw store (dev-session-config))   ;; drop it in the list
                (get! "/dashboard" dashboard))]
    (server-stop srv)
    0))
```

Swapping the backend is a one-line change:

```turmeric
(session-mw (file-store-new "/var/lib/app/sessions") (default-session-config))
```

A complete login/logout flow lives in
[`examples/login_app.tur`](examples/login_app.tur):

```sh
tur run spices/tourist-session/examples/login_app.tur
```

## How it works

`session-mw` does two things: it installs the store and cookie config into a
process-global (read once per request, set once before the server starts, just
like tourist's own dispatch state) and returns a tourist **after-middleware**
item. There is no separate "before" middleware — the session is loaded
**lazily** the first time a handler calls a `session-*` helper:

1. the configured cookie is read from the request;
2. if it names a session the store knows, that ID + map are adopted;
3. otherwise a fresh 256-bit ID and an empty map are minted.

On the way out, the after-middleware inspects the per-request session state:

| Situation | Action |
|-----------|--------|
| `session-destroy!` was called | `store-delete` + an expiring `Set-Cookie` |
| the session was written (dirty) | `store-save` + a fresh `Set-Cookie` |
| `rolling?` on, established session | `store-save` + `Set-Cookie` (slide expiry) |
| read-only / never touched | nothing — no needless writes or cookies |

Because loading is lazy, a request whose handler never touches the session
pays nothing and is handed no cookie.

## Public API

### Middleware

- `(session-mw store config)` → tourist item. Install once, place in the
  `tourist` list (not wrapped in `use!`).
- `(session-flush ctx resp)` → the after-middleware itself, exported so a
  non-tourist host can flush a session manually.

### Context helpers (call from handlers)

- `(session-get ctx key)` → `(Result cstr cstr)` — `ok` value or `err "missing"`
- `(session-set! ctx key val)` → mark dirty, store `val`
- `(session-del! ctx key)` → remove a key, mark dirty
- `(session-destroy! ctx)` → delete from the store + clear the cookie
- `(session-id ctx)` → the current session ID (64-char hex)

### Configuration

- `(default-session-config)` — production: `Secure`, `HttpOnly`, 24 h, `Lax`
- `(dev-session-config)` — development: `Secure` off (works over plain HTTP)
- `SessionConfig` fields: `cookie-name`, `path`, `domain`, `max-age`,
  `secure?`, `http-only?`, `same-site`, `rolling?`

### Stores

- `(memory-store-new)` → `Store`; `(memory-store-count store)` for tests
- `(file-store-new dir)` → `Store` (creates `dir` 0700 if absent)

## Writing a custom store

The store is a small vtable, not a typeclass — tourist registers middleware as
a bare C-ABI function pointer with no captured environment, so the middleware
reads its store from a process-global, and a global cannot hold a value of an
erased typeclass-instance type. A backend therefore provides three functions
and an opaque state pointer and calls `store-new`:

```turmeric
;; load   : (c-fn [state:int id:cstr] int)            -> SessMap handle, 0 = miss
;; save   : (c-fn [state:int id:cstr map:int ttl:int] int)
;; delete : (c-fn [state:int id:cstr] int)
(store-new my-load my-save my-delete my-state)
```

Session data is a `SessMap` (see `session/data`) — a compact owned
string→string map. Stores clone on the way in and out so a request's working
copy never aliases the master copy. This is the same open-set extensibility a
typeclass would give, expressed in tourist's established fn-pointer idiom.

## Security notes

- **Session IDs** are 32 bytes (256 bits) from the OS CSPRNG
  (`getentropy`, falling back to `/dev/urandom`), hex-encoded. The ID is a
  bearer token; it is not signed or encrypted.
- **Session fixation:** this version does not regenerate the ID after login.
  Until a `session-rotate!` lands, call `session-destroy!` and re-create the
  session after a privilege change.
- The file store requires a POSIX filesystem (atomic `rename`). Session IDs
  are hex so they are always safe path components.

## Status

| Backend | State |
|---------|-------|
| memory store | ✅ implemented + tested |
| file store | ✅ implemented + tested |
| Valkey/Redis store | ⏳ deferred — see [docs](../../docs) |

The Valkey store (plan phase SS8) is deferred: the repo already ships a full
[`valkey`](../valkey) client (hiredis-based, superseding the plan's minimal
RESP2 client in SS7), but a Valkey-backed store can only be verified against a
live server (the plan tags those tests `requires.valkey`, skipped in CI). The
memory and file stores already demonstrate the swappable-store thesis — two
backends behind one `Store` vtable.

## Tests

```sh
tur test spices/tourist-session/tests/session
```

Covers session ID generation, cookie parse/build, the `SessMap`, both stores
through the `Store` vtable, and the full ctx + middleware request lifecycle
(lazy load, dirty persist, destroy) driven against fabricated tourist
contexts.
