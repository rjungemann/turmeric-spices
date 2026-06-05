# tur-signal tests

`test_core.tur` is the only currently-runnable test module. The
per-module test matrix planned in `docs/upcoming/tur-signal-rebuild-plan.md`
(test_osc, test_filter, test_shaper, test_envelope, test_compose) is
blocked on a downstream codegen regression in `tur` itself, tracked at
`docs/reported/vec-typed-fat-closure-readback-fixture-regressed-codegen.md`
in the turmeric repo. That gap blocks any caller that:

- applies an SF (e.g. `((sine 1.0 0.0) input)`),
- reads back a typed-aggregate-returning closure result
  (e.g. `(let [p (ps 0.0)] (pair-fst p))`),

so the modules `osc`, `filter`, `shaper`, `envelope`, and `compose`
all `tur check` clean but cannot currently be exercised end-to-end
from a separate caller module.

When that report resolves, the per-module test files should land next
to `test_core.tur`. The rebuild plan documents the expected coverage
shape.

## Running

```sh
cd spices/signal
tur run tests/signal/test_core.tur
# expected: PASS test_core
```
