# tur-signal

Arrow-based signal processing for [Turmeric](https://github.com/rjungemann/turmeric): Signal Function (SF) combinators, DSP filters, ADSR envelopes, and synthesizer voices.

---

## Overview

`tur-signal` provides a composable signal processing pipeline built on Turmeric's Arrow typeclass:

- **`signal/core`** -- Signal type, SF arrow combinators (`constant`, `time-signal`, `sample`, `map-signal`, `pair-signals`)
- **`signal/dsp`** -- Oscillators (`sine`, `square`, `sawtooth`), filters (`low-pass`, `high-pass`), amplitude ops (`gain`, `mix`, `add`)
- **`signal/envelope`** -- Piecewise-linear ADSR envelope generator (`adsr-fixed`, `ADSRParams`)
- **`signal/synth`** -- Synthesizer voices and effects (`voice`, `fm-voice`, `wavetable-osc`, `karplus-strong`, presets, `effects-chain`, `step-sequencer`)

---

## Installation

Add `tur-signal` to your `build.tur` `:spices` block:

```turmeric
(defpackage my-app
  :spices #{
    "signal" #{:url    "https://github.com/rjungemann/turmeric-spices"
               :ref    "signal-v0.1.0"
               :subdir "spices/signal"}
  })
```

Then fetch:

```sh
tur fetch
```

---

## Quick Start

```turmeric
(import signal/core)
(import signal/dsp)
(import signal/envelope)

;; 440 Hz sine wave sampled at t=0.0
(let [sine-sf (sine 440.0 0.0)
      dummy   (constant ())
      out     (sine-sf dummy)]
  (println (out 0.0)))    ; => ~0.0

;; Chained: sine -> low-pass filter
(let [osc     (sine 440.0 0.0)
      filt    (low-pass 0.3)
      sig     ((osc (constant ())))
      filtered (filt sig)]
  (println (filtered 0.001)))

;; ADSR envelope
(let [params (ADSRParams 0.01 0.1 0.7 0.3)
      env    (adsr-fixed params 0.5)
      out    (env (constant ()))]
  (println (out 0.005)))  ; => ~0.5 (mid-attack)
```

---

## Module Reference

### `signal/core`

| Symbol | Description |
|--------|-------------|
| `constant val` | Signal that always returns `val` |
| `time-signal t` | Signal that returns `t` at each sample |
| `sample sig t` | Evaluate signal `sig` at time `t` |
| `map-signal f sig` | Lift pure function `f` over signal pointwise |
| `pair-signals sig-a sig-b` | Zip two signals into a Pair signal |
| `left-signal sig` | Identity injection (Left branch placeholder) |
| `right-signal sig` | Identity injection (Right branch placeholder) |

### `signal/dsp`

| Symbol | Description |
|--------|-------------|
| `sine freq phase` | Sine-wave SF at `freq` Hz with phase offset |
| `square freq duty` | Square-wave SF with duty cycle |
| `sawtooth freq` | Sawtooth-wave SF |
| `low-pass alpha` | First-order IIR low-pass filter SF |
| `high-pass alpha` | First-order IIR high-pass filter SF |
| `gain g` | Scale signal by constant factor `g` |
| `mix alpha` | Weighted mix of a Pair signal |
| `add` | Sample-wise sum of a Pair signal |

### `signal/envelope`

| Symbol | Description |
|--------|-------------|
| `ADSRParams attack decay sustain release` | ADSR parameter struct |
| `adsr-fixed params gate-duration` | Piecewise-linear ADSR envelope SF |

### `signal/synth`

Synthesizer voices, wavetable oscillators, granular synthesis, FM, Karplus-Strong string model, effects chain, and step sequencer. See source for full API.

---

## Examples

Examples live in `examples/`:

```sh
cd ../turmeric-spices/spices/signal
tur run examples/01_basics.tur
tur run examples/02_signals.tur
tur run examples/03_dsp.tur
```

---

## Development

```sh
cd spices/signal
tur check src/signal/synth.tur   # typecheck
tur run tests/signal/arrow_tests.tur  # run tests
```
