# tur-ws-core

Shared RFC 6455 WebSocket protocol types and handshake crypto for Turmeric.

`ws-core` is the small foundation that **`ws-client`** and **`ws-server`** are
both built on. It exists so the two spices speak a single set of types: the
connection handle `WsConn`, the frame view `WsFrame`, the frame accessors and
predicates, and the pure `ws-accept-key` handshake transform all live here,
once.

## Why (NAME-V0)

Before `ws-core`, `ws-client` and `ws-server` each defined their own
identically-named `WsConn` / `WsFrame` opaques. An application that wanted to
use both -- e.g. a proxy that accepts a server connection and dials an upstream
client connection -- could not `:refer`-import both, because the type names
collided. Hoisting the protocol-level types into `ws-core` gives the whole
program one `WsConn` and one `WsFrame`, importable from both sides without
`:as` aliasing.

> Note: the planning doc described `ws-client` / `ws-server` "re-exporting"
> these names. Turmeric modules can only export symbols they *define*, so the
> canonical types are imported directly from `ws-core` rather than re-exported
> through each spice. The user-visible result is the same: one `WsConn`, no
> collision.

## Modules

| Module | Exports |
|---|---|
| `ws-core/conn` | `WsConn` -- the opaque connection handle. |
| `ws-core/frame` | `WsFrame` plus `ws-frame-kind` / `ws-frame-data` / `ws-frame-len` / `ws-frame-text` and the `ws-text?` / `ws-binary?` / `ws-ping?` / `ws-pong?` / `ws-closed?` / `ws-timeout?` / `ws-error?` predicates. |
| `ws-core/handshake` | `ws-accept-key` -- `base64(sha1(key + GUID))`, the RFC 6455 4.2.2 accept transform. |

## Usage

Most users never depend on `ws-core` directly -- they use `ws-client` or
`ws-server`, which pull it in. When you need the shared types by name (for
instance to write a helper over a `WsConn`, or to import both spices at once),
import them from `ws-core`:

```turmeric no-check
(import ws-core/conn  :refer [WsConn])
(import ws-core/frame :refer [WsFrame ws-text? ws-frame-text])
(import ws-client/client :refer [ws-connect ws-recv])
(import ws-server/server :refer [ws-upgrade ws-server-send])
```

`WsConn` is an opaque `:int` handle. The underlying C struct differs by side
(the client points it at its connection struct, the server at its own), but
that is an implementation detail behind the handle; each spice's operations
know which struct their handles wrap.

## Tests

`tests/accept_key_test.tur` checks `ws-accept-key` against the canonical
RFC 6455 4.2.2 vector. Run with `tur test tests`.
