# Changelog

## Unreleased

### Changed

- Dropped two of the three compiler workarounds now that their upstream reports
  are fixed. `decode-mp-list` / `__mp-arr-decode` call `unwrap` directly again
  instead of routing through the concrete `__mp-arr-len` / `__mp-arr-node`
  shims (`generic-unwrap-specializes-by-the-enclosing-type-argument`), and
  `tests/container-round-trip.tur` drops the eight outer `(:: ... Buf)`
  ascriptions from its `@Cons`-pinned `encode-mp` calls
  (`pinned-instance-dispatch-loses-an-opaque-return-type`).
- The four primitive `DecodeMp` instances are one-line forwards to
  `mp-get-int` / `mp-get-str` / `mp-get-bool` / `mp-get-float` again, replacing
  ~70 lines of hand-written carrier-shaped inline C and its duplicated error
  strings. This needed a second compiler fix beyond the one filed: restoring
  the forwards exposed
  `instance-method-call-inside-an-instance-body-takes-the-enclosing-result-type`
  (rjungemann/turmeric#874), where a method called from inside another instance
  body of the same class was specialized with the ENCLOSING instance's result
  type. **Requires a `tur` at or after that fix.**

  With all three gone, the spice carries no compiler workarounds.

## 0.1.0

Initial release: MessagePack binary serialization, the binary twin of tur-json.

### Added

- `EncodeMp` / `DecodeMp` / `DecodeMpChecked` typeclasses, all format-tagged in
  both the class and the method name. Two classes declaring the same method
  name dispatch by declaration order with no diagnostic, so the naming is what
  keeps this spice and tur-json apart -- not a style choice.
- `Buf` in `msgpack/buf` -- an owned, length-prefixed byte buffer laid out as
  `{ int64 len; uint8 data[] }`, matching stdlib `serial.tur`'s bytes value so
  the two interoperate at the pointer level. `buf-concat` consumes both
  operands, which is the whole of how fragments become a container.
- `msgpack/decode` -- `MpTree` / `MpNode` as real opaques (not json's bare
  `:int` handles), `mp-parse`, `mp-tree-root`, `mp-map-get`, `mp-arr-get`,
  shape predicates, and typed reads that return a `Result` rather than
  substituting a zero.
- Primitive `EncodeMp` / `DecodeMp` instances for `int`, `bool`, `float`,
  `cstr`, `(Option A)` and `(Cons A)`, plus the standalone `decode-mp-list`.
- The derive family: `derive-msgpack` (+ `-encode` / `-decode`),
  `derive-msgpack-opaque` (+ halves), and `derive-msgpack-sum` (+ halves).
  Structs encode as a map with string keys; sums are externally tagged as
  `{"Ctor": {...}}` -- the same shapes tur-json uses.
- `derive-mp-decoder` / `MpDecodeErrors` -- the accumulating validator, which
  collects every field violation with its key, expected type and actual type
  instead of stopping at the first. Vocabulary mirrors `stdlib/schema.tur`,
  with msgpack `map` reported as "object" so a violation reads identically to
  the json spice's on the same struct.

### Notes

- **No native dependencies.** Both halves of the codec are vendored C
  (`c/msgpack/mp_core.c`, reached through `:c-sources`). The plan named
  ludocode/mpack for the read path; MP0 measured it and hand-rolled the reader
  instead -- mpack ships no `CMakeLists.txt` at any tag, its header-driven
  configuration skews against turmeric's bare-`cc` compilation of emitted C,
  and it would have been a dependency for only half the codec. README.md has
  the full reasoning.
- `mp-parse` validates the entire buffer before handing out any node, so every
  accessor downstream is total and malformed input is rejected at the boundary.
- Golden bytes in `tests/` were cross-generated with CPython's `msgpack`; the
  reader was swept under ASan/UBSan over every single-byte mutation and
  truncation of a document corpus plus 200k random inputs.
- Three compiler defects were worked around to ship this; each is filed
  upstream and each workaround is marked `WORKAROUND` in place, naming the
  report and what to delete when it lands. (All three are gone as of
  Unreleased, above.)
