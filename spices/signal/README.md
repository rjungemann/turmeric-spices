# tur-signal

Typed Signal Function (SF) library for [Turmeric](https://github.com/rjungemann/turmeric):
oscillators, filters, shapers, ADSR envelopes, and SF-pipeline composition.

---

## Status

The Tier 1 surface is written and `:exports` is pruned to match, but
the spice is currently not exercisable end-to-end against turmeric
`main`. Two distinct gaps both have to clear before any caller (the
Phase 1 example, `test_core`, or downstream importers) works:

1. `^fat name : (fn ...)` parameter annotations are dropped across the
   `(defmodule ... (export ...))` boundary, so any caller of an
   exported fat-typed defn (`sample`, every oscillator/filter/shaper
   SF, `effects-chain`) gets a `TUR-E0001` `expected (fn [] : ?)` at
   the call site. Tracked at
   `docs/reported/defmodule-loses-fat-fn-type-annotation.md` in the
   turmeric repo. The same bug also breaks `signal/compose`'s
   in-module `(__apply-sf ...)` self-call, so `tur check
   src/signal/compose.tur` itself fails today.
2. Once (1) clears, the SF-pipeline shape still needs the
   `int<->ptr<void>` carrier bridge fix tracked at
   `docs/reported/vec-typed-fat-closure-readback-fixture-regressed-codegen.md`
   (it produces 4 `-Wint-conversion` errors at the apply-sf body and
   the `vec-push!` call sites).

`src/signal/{core,osc,filter,shaper,envelope}.tur` all `tur check`
clean (after the `pair` / `(as float ...)` workarounds the rebuild
applied to dodge two other defmodule scoping gaps for `Pair`).
`compose.tur` does NOT check clean. The Phase 1 example does NOT run.
`tests/signal/test_core.tur` does NOT pass. The Tier 1 surface
remains the *intended* shipping shape and the source layout matches
the plan; only the runnability gates remain.

This is the surface area an honest version of the previous spice
would have shipped. Tier 2 -- wavetable/FM/Karplus-Strong/granular,
voice / poly-synth / step-sequencer, resonant filters -- is
explicitly out of scope until each lands behind a real consumer
and a dedicated plan. See
`docs/upcoming/tur-signal-rebuild-plan.md` in the turmeric repo.

---

## Module layout

| Module            | Surface                                                                                  |
|-------------------|------------------------------------------------------------------------------------------|
| `signal/core`     | `constant`, `time-signal`, `sample`, `map-signal`, `pair-signals`                        |
| `signal/osc`      | `sine`, `square`, `sawtooth`, `triangle`                                                 |
| `signal/filter`   | `low-pass`, `high-pass`                                                                  |
| `signal/shaper`   | `gain`, `offset`, `invert`, `abs-sf`, `scale`, `saturate-tanh`, `hard-clip`, `clip`, `mix`, `add`, `multiply` |
| `signal/envelope` | `ADSRParams`, `adsr-fixed`, `adsr-gen`                                                   |
| `signal/compose`  | `effects-chain`                                                                          |

A `Signal Sample` is `(fn [t :float] :float)`. An `SF Sample Sample`
is the closure `(fn [^fat sig : (fn [float] float)] (fn [t :float] :float ...))`
returned by every shaper / filter / oscillator constructor: apply it
to a signal to get a signal.

Pair-consuming mixers (`mix`, `add`, `multiply`) consume the
`Signal (Pair Sample Sample)` shape produced by `pair-signals`.

---

## Installation

In your `build.tur` `:spices` block:

```turmeric
(defpackage my-app
  :spices #{
    "signal" #{:url    "https://github.com/rjungemann/turmeric-spices"
               :ref    "signal-v0.1.0"
               :subdir "spices/signal"}
  })
```

Then `tur fetch`.

---

## Quick start

The Phase 1 example, working today:

```turmeric
(defmodule my/app
  (import signal/core :refer [constant time-signal sample])

(defn main [] : int
  (let [c (constant 0.5)]
    (println (sample c 0.0))                ;; 0.5
    (println (sample c 9999.0)))            ;; 0.5
  (println (time-signal 0.25))              ;; 0.25
  0)

) ;; end defmodule
```

The Phase 2-5 patterns (oscillators, filters, shapers, envelopes,
`effects-chain`) are written and `tur check`-clean but blocked
from caller-side exercise until the upstream codegen fix lands.
The intended call shapes look like:

```turmeric
;; oscillator at t = 0.25
(let [unused (constant 0.0)
      s1     ((sine 1.0 0.0) unused)]
  (sample s1 0.25))                          ;; ~1.0 (sin pi/2)

;; sine driven through a low-pass filter
(let [unused (constant 0.0)
      tone   ((sine 440.0 0.0) unused)
      tone-f ((low-pass 0.3) tone)]
  (sample tone-f 0.001))

;; ADSR envelope
(let [params (make-struct ADSRParams 0.01 0.1 0.7 0.3)
      env-sf (adsr-fixed params 0.5)
      env    (env-sf (constant 0.0))]
  (sample env 0.005))                        ;; ~0.5 (mid-attack)

;; effects chain
(let [v   (vec-new)]
  (vec-push! v (gain 0.5))
  (vec-push! v (low-pass 0.3))
  (let [^fat out : (fn [float] #{} float)
                (effects-chain v ((sine 440.0 0.0) (constant 0.0)))]
    (out 0.0)))
```

---

## Examples

```sh
cd spices/signal
tur run examples/01_constant_and_time.tur
# Currently fails at (sample c 0.0) -- see Status above.
```

---

## Development

```sh
cd spices/signal
for f in src/signal/*.tur; do tur check "$f"; done
# 5/6 clean; compose.tur fails -- see Status above.
tur run tests/signal/test_core.tur
# Currently fails -- see Status above.
```
