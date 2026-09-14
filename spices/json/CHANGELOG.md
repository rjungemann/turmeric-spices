# Changelog

## 0.4.0

### Changed

- **BREAKING -- the serde typeclasses are now format-tagged.** Turmeric
  typeclasses resolve globally, so a program that holds both this spice and
  `spices/msgpack` cannot have two classes named `Encode`. Neither spice gets
  the unqualified spelling: json's classes and methods now carry a `Json` /
  `-json` tag, matching msgpack's `EncodeMp` / `encode-mp`.

  | Before (0.3.0) | After (0.4.0) |
  | --- | --- |
  | `Encode` / `encode` | `EncodeJson` / `encode-json` |
  | `Decode` / `decode` | `DecodeJson` / `decode-json` |
  | `DecodeChecked` / `decode-checked` | `DecodeJsonChecked` / `decode-json-checked` |
  | `decode-list` | `decode-json-list` |
  | `derive-decoder` | `derive-json-decoder` |
  | `encode-string` | `encode-json-string` |
  | `DecodeErrors` | `JsonDecodeErrors` |
  | `decode-errors-count` | `json-decode-errors-count` |
  | `decode-error-path` | `json-decode-error-path` |
  | `decode-error-expected` | `json-decode-error-expected` |
  | `decode-error-got` | `json-decode-error-got` |
  | `decode-errors-free` | `json-decode-errors-free` |

  The `derive-json*` macro family is unchanged -- those names were already
  format-explicit. `derive-decoder` was the one bare derive macro, and it
  collided head-on with msgpack's `derive-mp-decoder`, so it moved with the
  classes.

  The exported *defns and types* in the second half of the table
  (`encode-string`, `DecodeErrors`, and the `decode-error*` accessors) were
  not strictly forced to move -- `:refer` disambiguates a defn, unlike a
  class. They moved anyway, in this release rather than a later one, because
  a reader holding both spices open would otherwise still see two
  `DecodeErrors`, and splitting the rename across two breaking releases buys
  nothing.

  **Method names had to move too, not just class names.** Two classes
  declaring the same method name compile with zero diagnostics and dispatch
  to whichever instance registered last, so leaving both spices with a method
  called `encode` would let an unrelated reordering silently flip which
  format a program serializes to. See
  [same-method-name-in-two-classes-dispatches-by-declaration-order](https://github.com/rjungemann/turmeric/blob/main/docs/reported/same-method-name-in-two-classes-dispatches-by-declaration-order.md).
  Until that grows an ambiguity diagnostic, distinct method names are the
  only thing keeping the two spices apart.

  There is no deprecation window, no alias, and no shim: this is a record of
  what changed, not a migration path. Downstream in-repo consumers (`http`,
  `httpd`) moved in the same commit.
