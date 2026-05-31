# CI Failure Remediation Plan

> **Status:** Active / in-progress
> **Last Updated:** 2026-05-31
> **Source:** Local `tur` run against `main` (compiler built from `rjungemann/turmeric` tip-of-main)

Five distinct failure classes came out of a full scan of `main`. They are ordered
roughly by blast radius — fix A first because it masks every other test result.

---

## A. Test runner can't find tests in subdirectories

> **Status:** ✅ Done — implemented **Option 1** (updated the CI "Run spice
> tests" step). If `tests/` has flat `.tur` files it runs `tur test tests`
> (covering flat spices and the linalg/template aggregators); otherwise it
> descends and runs `tur test` on every directory that directly contains a
> `.tur` file (covering frame-style 1-level nesting and tourist's 2-level
> fixtures). Verified locally that the runner now finds and attempts tests in
> every spice with a `tests/` dir — no more "no .tur files found". Template's
> `fixtures/` (run by `run-fixtures.sh`, not `tur test`) are deliberately left
> alone. Suites that still fail now do so for the §B/§D reasons below.

### Symptom

`tur test tests` exits non-zero with:

```
tur: no .tur files found in 'tests'
```

### Root cause

The CI step (`.github/workflows/ci.yml`, "Run spice tests") runs:

```sh
"$TUR" test tests
```

from inside each spice directory. `tur test` only looks at `.tur` files at the
**top level** of the path it is given. Many spices store their test files one
level deeper:

```
tests/
  <spice>/
    foo_test.tur
    bar_test.tur
```

### Affected spices

`c-dsl`, `frame`, `glsl`, `httpd`, `osc`, `plutovg`, `png`, `rtaudio`, `rtmidi`,
`signal`, `tidal`, `valkey`, `wav`, `opengl`, `plot`, `stats`, `postgres`

### Fix options (pick one)

**Option 1 — Update the CI step** to pass the subdirectory explicitly.  
Change the CI "Run spice tests" step to discover and pass the right path:

```sh
test_dir="tests"
if [ ! -f tests/*.tur ] && [ -d "tests/$(basename $(pwd))" ]; then
  test_dir="tests/$(basename $(pwd))"
fi
"$TUR" test "$test_dir"
```

**Option 2 — Move all test files** to `tests/` directly (drop the extra level).  
This is simpler long-term but requires touching every affected spice.

Note: some of these spices also have real test-logic failures (see §D) that
will surface once the runner can find the files.

---

## B. Type-check failures

These are caught by `tur check` and block compilation.

### B1. `watch/watch.tur` — unterminated `defmodule`

> **Status:** ✅ Done — added the missing closing `)` for the `defmodule`.
> `watch/watch.tur` now checks clean, and `notebook/main.tur` + `cli.tur`
> (which transitively import it) now check clean too. **Newly surfaced:** with
> the source fixed, the `watch` *test* suite now fails at C compile time
> because its inline-C helpers use `usleep`/`useconds_t`/`FILE` without the
> needed feature-test macro and headers — tracked under §E4a.

**Error:**
```
watch/src/watch/watch.tur:29:1: error: unterminated list (missing ')')
```

The file is 909 lines and the `defmodule watch/watch` that opens on line 29
is missing its closing `)`. The last line of the file needs an additional `)`.

**Fix:** Add a closing `)` at the end of `spices/watch/src/watch/watch.tur`.

---

### B2. `ok-val` receives `ptr<void>` — affects `plot`, `plutovg`, `wav`

> **Status:** ✅ Done (the `ok-val`/`ptr<void>` mismatch itself). These spices
> roll their own `Result` encoding — a malloc'd `{is_ok, ok_val, err_val}`
> struct returned as an opaque handle — but the call sites used the stdlib
> `ok?`/`ok-val`/`err-val`, which only accept the stdlib `Result` type.
>
> Fix applied:
> - **plot**: `plot/core.tur` defines its own encoding (`__ok`/`__err`); added
>   local `__ok?`/`__ok-val`/`__err-val` accessors and switched the 11 call
>   sites to them. `plot/core.tur` now checks clean.
> - **plutovg/wav (tests)**: added reusable `result-ok?`/`result-val`/
>   `result-err` accessors to `test/assert` (co-located with `assert-ok`,
>   which already reads this struct), exported them in `test/assert.tur` and
>   `test/build.tur`, and switched the test call sites off the stdlib
>   `ok?`/`ok-val`.
>
> **Downstream issues surfaced once the type error cleared:**
> - **wav**: type error gone; 2/3 suites pass. `info_test` now fails only on a
>   missing `sndfile.h` → tracked under §E.
> - **plot**: `core.tur` compiles, 3/6 suites pass. `interval`/`line`/`point`
>   tests define a local `pair` that shadows the stdlib `pair` (same class as
>   §D2) → tracked under §D7.
> - **plutovg**: type error gone, but the suites use multi-statement `(it ...)`
>   bodies that end in cleanup (void), while `it` is `(defn it [desc result
>   :bool])`. ~40 `it` blocks need restructuring to return a bool → tracked
>   under §D8 (needs a decision, see Priority order).

**Error (representative):**
```
plot/src/plot/core.tur:1151:31: error [TUR-E0001]:
  function 'ok-val' arg 1: expected (type-app (type-app Result tyvar) tyvar), got ptr<void>
```

The same pattern appears in `spices/plutovg/tests/plutovg/canvas_test.tur:31`
and `spices/wav/tests/wav/info_test.tur:12`. These call `ok-val` on values that
are typed as `ptr<void>` (the old untagged-union Result encoding) rather than
the current `Result` type.

**Fix:** Each call site needs to unwrap via a `ptr<void>` accessor instead of
`ok-val`, or the source module needs to return a typed `Result` so that `ok-val`
can see the correct type argument. Audit every `ok-val` / `ok?` call in the
three spices and verify the called function's declared return type matches the
current `Result` encoding.

---

### B3. `linalg` — three independent issues in `src/linalg/`

**B3a. `malloc`/`free` unbound:**
```
linalg/src/linalg/vec.tur:6:10: error [TUR-E0003]: unbound symbol 'malloc'
```
`(declare malloc free)` is used but these symbols are not available without
a `(import ...)` or an inline-C block that includes `<stdlib.h>`. Add the
appropriate declaration or use the stdlib wrappers.

**B3b. `defstruct` field list syntax:**
```
linalg/src/linalg/vec.tur:13:3: error: defstruct field list must be a vector [f1 : T1 f2 : T2 ...]
```
The struct fields are written as plain S-expressions instead of a vector
literal. Change to bracket syntax:
```turmeric
(defstruct vec [len :int  data :ptr<float64>])
```

**B3c. `vec-new` name conflicts with stdlib:**
```
error: defn: 'vec-new' is already defined by an auto-loaded stdlib module
```
Rename the local definition (e.g. `linalg-vec-new`) or qualify it with the
module name so it does not shadow the stdlib symbol.

---

### B4. `opengl/shaders.tur` — variadic body with inline-C

**Error:**
```
opengl/src/opengl/shaders.tur:75:3: error:
  defn 'shader-program': variadic body contains inline-C;
  inline-C blocks need a fixed arity signature
```

The compiler rejects inline-C blocks inside variadic (`& args`) functions.

**Fix:** Give `shader-program` a fixed arity, e.g. accept a list or a vector
of shaders rather than a variadic rest argument.

---

### B5. `sdf-raylib/integration.tur` — unbound `scene-glsl`

**Error:**
```
sdf-raylib/src/raylib/integration.tur:537:19: error [TUR-E0003]: unbound symbol 'scene-glsl'
```

`scene-glsl` is referenced at line 537 (`(csdf-glsl-free scene-glsl)`) but
is never defined in the module. It was likely meant to be the return value of
an earlier call captured in a `let` binding.

**Fix:** Wrap the relevant code block in a `let` that binds `scene-glsl` before
the call to `csdf-glsl-free`.

---

### B6. `stats` — `static` functions in included header (`pcg32.h`)

**Error:**
```
stats/src/stats/pcg32.h:23:20: error: invalid storage class for function 'pcg32_srandom_r'
```

`pcg32.h` defines functions with `static inline` storage in a header that is
`#include`d inside a `defmodule` body. The Turmeric C emitter wraps defmodule
bodies in a C function scope, making `static` function definitions inside that
scope illegal.

**Fix:** Move the `#include "pcg32.h"` to a top-level inline-C block (outside
the `defmodule`) using the file-level `c-preamble` mechanism, or convert the
affected definitions to `extern` with a matching `.c` compilation unit.

---

### B7. `notebook` — cannot resolve `ansi/term`

> **Status:** ✅ Done via §C (added `spices/ansi` to root `:members`).

**Error:**
```
notebook/src/notebook/tui.tur:12:3: error: module 'ansi/term' not found
```

`notebook/tui.tur` imports `ansi/term`, which lives in the `ansi` spice.
`ansi` is not listed in the root `build.tur` `:members`, so workspace
resolution cannot find it (see also §C).

**Fix:** Add `"spices/ansi"` to `:members` in the root `build.tur` (§C fixes
this for all affected spices at once).

---

## C. New spices missing from root workspace `:members`

> **Status:** ✅ Done — all ten added to root `build.tur` `:members`. Verified
> `notebook/tui.tur` now resolves `ansi/term` (B7 fixed). `notebook/main.tur`
> and `cli.tur` still fail, but only because they transitively import the
> broken `watch/watch.tur` (B1).

The following spice directories exist on disk but are absent from the root
`build.tur` `:members` list:

| Spice | Impact |
|---|---|
| `ansi` | `notebook` cannot resolve `ansi/term` (B7) |
| `c-dsl` | Not in workspace; cross-spice imports unresolvable |
| `frame` | Not in workspace |
| `glsl` | Not in workspace |
| `linalg` | Not in workspace |
| `opengl` | Not in workspace |
| `raygui` | Not in workspace |
| `sdf-raylib` | Not in workspace |
| `signal` | Not in workspace |
| `stats` | Not in workspace |

**Fix:** Add all ten to `build.tur` `:members`:

```turmeric
:members ["spices/ansi"
          "spices/c-dsl"
          "spices/frame"
          "spices/glsl"
          "spices/linalg"
          "spices/opengl"
          "spices/raygui"
          "spices/sdf-raylib"
          "spices/signal"
          "spices/stats"
          ;; ... existing members ...
          ]
```

---

## D. Test logic bugs

These are bugs in the test files themselves (or their immediate dependencies),
not in the spice source. They will surface once §A is resolved.

### D1. `rtaudio` / `rtmidi` — `defmodule` accidentally inside comment

> **Status:** ✅ Done — split the opening line of all 7 affected test files so
> `(defmodule ...)` starts on its own line. Both suites now compile and pass
> (`rtaudio` 3/3, `rtmidi` 4/4).

**Error:**
```
rtaudio/tests/rtaudio/core_test.tur:13:7: error: unexpected ')'
```

The first line of every `rtaudio` and `rtmidi` test file reads:

```
;; tests/rtaudio/core_test.tur -- tests for rtaudio/core.(defmodule rtaudio/tests/core
```

The `(defmodule ...)` form got appended to the end of the comment line. Because
`;;` comments to end-of-line, the `defmodule` is never opened. The file body
(imports, describe, defn) is at the top level, and the closing `)` that was
meant to close the defmodule is unmatched — hence "unexpected ')'".

**Fix:** Break the opening of each test file so `(defmodule ...)` is on its
own line:

```turmeric
;; tests/rtaudio/core_test.tur -- tests for rtaudio/core.
(defmodule rtaudio/tests/core
  ...
```

Affects all `.tur` files under `tests/rtaudio/` and `tests/rtmidi/`.

---

### D2. `tidal` — local `ok` shadows stdlib

**Error:**
```
tidal/tests/tidal/tidal_test.tur:43:9:
  error: defn: 'ok' is already defined by an auto-loaded stdlib module
```

The test defines `(defn ok ...)` and `(defn ok? ...)` and `(defn ok-val ...)`
as `ptr<void>`-based helpers (the old Result encoding). These now conflict with
the stdlib `ok` / `ok?` / `ok-val` from the current `Result` type.

**Fix:** Either rename the local helpers (e.g. `tidal-ok`) or port the test
to use the stdlib `Result` constructors directly.

---

### D3. `c-dsl` — `cons` cell used as `cstr`

**Error (from `tests/c-dsl/codegen_test.tur:51`):**
```
error: expression in call head has type `cstr`, which is not callable
```

The test passes a `cons` cell (a linked-list node with `int` tag) as an
argument where a `cstr` is expected. The `c-join` function signature or the
test call site is mismatched.

**Fix:** Verify `c-join`'s declared signature in `src/c-dsl/core.tur` and
update the test to build the argument list in the way `c-join` actually expects.

---

### D4. `glsl` — `test/assert` not found

**Error:**
```
glsl/tests/glsl/codegen_test.tur:11:3: error: module 'test/assert' not found
```

The `glsl` spice `build.tur` does not declare a `:spices` dependency on the
`test` spice, so `tur check` cannot resolve `test/assert`.

**Fix:** Add `"test"` to the `:spices` map in `spices/glsl/build.tur`:

```turmeric
:spices {"test" {:path "../test"}}
```

---

### D5. `opengl` — `assert-true` called with `int`

**Error (from `tests/opengl/buffers_test.tur:28`):**
```
error [TUR-E0001]: function 'assert-true' arg 1: expected bool, got int
```

The test passes an `int`-typed expression to `assert-true`, which requires
`bool`. OpenGL functions like `glIsBuffer` / `glIsVertexArray` return `int`
(0 or 1) in the generated bindings.

**Fix:** Wrap calls with an explicit cast: `(assert-true (not= 0 <gl-call>))`,
or declare the return type of the relevant FFI functions as `:bool`.

---

### D6. `signal` — C codegen produces wrong struct return type

**Error (in compiled output):**
```
error: incompatible types when returning type 'Pair__int__int' but 'int64_t' was expected
```

The `signal/arrow_tests.tur` test triggers a codegen bug where a function
returning `Pair<int, int>` is emitted with an `int64_t` return type. This is
likely a compiler-side issue (report upstream to `rjungemann/turmeric`) but
the test file or the spice source may be able to work around it by choosing
a struct type that the current emitter handles cleanly.

---

### D7. `plot` — local `pair` shadows stdlib (surfaced after §B2)

> **Status:** Open — same class as §D2.

`tests/plot/interval_test.tur`, `line_test.tur`, and `point_test.tur` each
define a local `(defn pair [x :float y :float] :int ...)` that collides with
the auto-loaded stdlib `pair`:

```
error: defn: 'pair' is already defined by an auto-loaded stdlib module
```

**Fix:** Rename the local helper (e.g. `plot-pair` or `__pair`) in those test
files. `plot/core.tur` itself compiles; 3 of 6 plot suites already pass.

---

### D8. `plutovg` — `(it ...)` bodies don't return `bool` (surfaced after §B2)

> **Status:** Open — needs a decision (see Priority order). Out of original
> plan scope.

`test/suite`'s `it` is `(defn it [desc :cstr result :bool] :bool)` — its
second argument is an eagerly-evaluated bool. The plutovg suites instead pass
multi-statement `(let ...)` bodies that perform asserts and end in resource
cleanup (`(surface-destroy s)` etc.), which return `:void`:

```
error [TUR-E0001]: function 'it' arg 2: expected bool, got nil
```

There are ~40 such `(it ...)` blocks across the plutovg tests. Two possible
directions:

1. **Restructure each test body** to compute a single bool pass-value, run
   cleanup, then return the bool. Faithful, but laborious and amounts to
   authoring test logic for each case.
2. **Change the framework**: give `test/suite` an `it`-like form that accepts
   a void/any body (e.g. a macro that treats reaching the end without a failed
   assert as a pass). This changes how the suite tally is computed and affects
   every spice.

This was masked by the B2 `ok-val` error and is independent of it.

---

## E. Missing native C libraries

These spices fail to compile their tests because system headers or libraries
are absent in the sandbox. They would also fail in CI unless the cmake-deps
fetch step installs them.

| Spice | Missing | Notes |
|---|---|---|
| `httpd` | `mbedtls/net_sockets.h` | `mbedtls` must be listed under `:cmake-deps` and fetched before the test step runs |
| `postgres` | `libpq-fe.h` | Requires a PostgreSQL client dev package or a bundled `libpq` cmake dep |
| `raygui` | raygui headers | `raygui` cmake dep must be fetched; CI fetch step skips if `:cmake-deps` absent from `build.tur` |
| `ansi` (`image_test`, `term_test`) | `struct sigaction` incomplete | Likely a missing `_POSIX_C_SOURCE` or `_DEFAULT_SOURCE` feature-test macro in the inline-C preamble |
| `wav` (`info_test`) | `sndfile.h` | Surfaced after §B2; needs `libsndfile` dev headers (system package or cmake dep). The other 2 wav suites pass. |

For `httpd`, `postgres`, and `raygui`, verify that `:cmake-deps` is present
in each `build.tur` and that the CI fetch step is not being skipped.

For `ansi`, add `#define _DEFAULT_SOURCE` (or `_POSIX_C_SOURCE 200809L`)
before the `#include <signal.h>` in the relevant inline-C block to expose
`struct sigaction` on glibc systems.

### E4a. `watch` test suite — inline-C missing feature macro / headers

> **Status:** Open — surfaced after the §B1 fix.

Once `watch/watch.tur` compiles, the `watch` test suite (8 of 9 files) fails
at the C compile step:

```
tests_backend_drain_into_test_tur.c:21: error: 'useconds_t' undeclared
... warning: implicit declaration of function 'usleep'
... error: unknown type name 'FILE'
```

The test helpers embed inline-C that calls `usleep` and uses `FILE`/`fopen`
without the feature-test macro and headers. Same class as the `ansi` row.

**Fix:** In the inline-C blocks of the affected `watch` test files, add
`#define _DEFAULT_SOURCE` and `#include <unistd.h>` / `#include <stdio.h>`
(or hoist these into a shared test preamble).

---

## Priority order

1. **§C** — Add missing workspace members to root `build.tur`. Unblocks B7 and
   lets cross-spice imports resolve. Low risk, mechanical change.

2. **§A** — Fix the test runner path so CI can actually find test files.
   Either update CI or move the files. Unblocks all §D findings.

3. **§B1** — Add closing `)` to `watch/watch.tur`. One-line fix, unblocks all
   `watch` tests.

4. **§D1** — Fix `rtaudio`/`rtmidi` comment-line bug. Mechanical, unblocks
   those test suites.

5. **§B2** — Fix `ok-val` / `ptr<void>` type mismatch in `plot`, `plutovg`,
   `wav`. Requires understanding the current `Result` encoding.

6. **§D2, §D4** — `tidal` name conflict and `glsl` missing dep. Small fixes.

7. **§B3, §B4, §B5, §B6** — Remaining type-check failures in `linalg`,
   `opengl`, `sdf-raylib`, `stats`. Each is self-contained.

8. **§D3, §D5, §D6** — Remaining test-logic bugs (`c-dsl`, `opengl`, `signal`).

9. **§E** — C library / header issues. Verify cmake-deps are wired up; add
   feature-test macros where needed.
