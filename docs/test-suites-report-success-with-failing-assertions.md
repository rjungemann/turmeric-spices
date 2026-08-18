# Every spice's test suite reports success even when assertions fail

**Severity:** high (CI is green on assertion failures across the whole repo;
only *compile* failures are actually caught)
**Found:** 2026-08-18, while building `spices/secret` -- it hid a genuinely
failing assertion in that spice's hex tests, which only surfaced because the
TAP lines were being read by eye.
**Fixed in:** `spices/secret` and `spices/test` (this change). **Every other
spice is still affected.**

## Summary

`tur test` judges a test file by the exit code of the program it builds --
its own help text says so:

```
Each test compiles and runs; it passes iff both succeed (exit 0).
```

But the conventional `main` every spice uses ends in a hardcoded `0`:

```turmeric
(defn main [] : int
  (__all-tests)
  (run-all)
  0)            ;; <-- always 0, whatever happened
```

and `run-all` is `: void` -- it prints the TAP summary and returns nothing.
So a failing `it` prints `not ok`, `run-all` prints `# N passed, M failed.`,
and then the process exits 0 and `tur test` reports the file as **passed**.

Observed directly:

```
$ tur test tests
not ok 2 - encodes the digit/letter boundaries
...
5 tests, 5 passed, 0 failed
$ echo $?
0
```

The consequence is that `.github/workflows/ci.yml`'s "Run spice tests" step
has only ever verified that test files **compile and do not crash**. Any
assertion regression in any spice passes CI silently.

## Fix

`test/runner` gains `run-all-status`, which prints the same summary and
returns a process status (additive -- `run-all` is unchanged, so no existing
spice breaks):

```turmeric
(defn run-all-status [] : int
  (do
    (__runner-print (__ts 4) (__ts 0) (__ts 2))
    (if (> (__ts 2) 0) 1 0)))
```

A test file then ends:

```turmeric
(defn main [] : int
  (__all-tests)
  (run-all-status))
```

Verified in both directions in `spices/secret`: with a deliberately broken
expected value the suite now reports `5 tests, 4 passed, 1 failed` and exits
1; restored, it exits 0.

## Remaining work

`spices/secret` is converted. Every other spice still ends its `main` with a
literal `0` and needs the same two-line change:

```sh
grep -rln "(run-all)" spices/*/tests/
```

As of this report that is **119 test files across 28 spices**: c-dsl, frame,
glsl, http, httpd, json, opengl, osc, plot, plutovg, png, postgres, raylib,
regex, rtaudio, rtmidi, scscm, stats, template, tourist, tourist-session,
tourist-session-valkey, valkey, wav, ws-client, ws-core, ws-server, zlib.

This is mechanical, but it should be expected to **turn some suites red** --
that is the point, and those failures are pre-existing rather than caused by
the change. Worth doing spice-by-spice rather than in one sweep, so each red
suite can be triaged on its own.
