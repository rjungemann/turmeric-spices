# tur-wav

WAV and PCM audio file read/write for Turmeric via libsndfile.

## Overview

`tur-wav` is a `cmake-dep` spice that wraps libsndfile. It exposes a small
file-oriented surface: `wav-open-read` returns a handle you can `wav-seek`
and `wav-read-float` from, and `wav-open-write` / `wav-write-float` pairs
the same shape for output. An `info` module reports sample rate, channel
count, and frame count.

Use it to load samples for `tur-rtaudio` streams, record output to disk, or
batch-process audio offline.

## Install

```turmeric
:spices {
  "wav" {:url    "https://github.com/rjungemann/turmeric-spices"
         :ref    "wav-v0.1.0"
         :subdir "spices/wav"}
}
```

## Quick start

```turmeric
(import wav/reader :refer [wav-open-read wav-close wav-read-float])
(import wav/info   :refer [wav-sample-rate wav-channels wav-frame-count])

(let [r (wav-open-read "in.wav")]
  (when (ok? r)
    (let [w (ok-val r)]
      (println (wav-sample-rate w) (wav-channels w) (wav-frame-count w))
      (wav-close w))))
```

```sweet-exp
#lang sweet-exp
import wav/reader :refer [wav-open-read wav-close wav-read-float]
import wav/info   :refer [wav-sample-rate wav-channels wav-frame-count]

let [r wav-open-read("in.wav")]
  when ok?(r)
    let [w ok-val(r)]
      println(wav-sample-rate(w) wav-channels(w) wav-frame-count(w))
      wav-close(w)
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/wav>
