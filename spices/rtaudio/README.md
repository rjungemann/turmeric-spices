# tur-rtaudio

Cross-platform audio I/O for Turmeric via RtAudio. Enumerate devices, open
input/output streams, and drive audio with a Turmeric callback or pull loop.

## Overview

`tur-rtaudio` is a `cmake-dep` spice that wraps RtAudio. It exposes the
common audio surface: query the available audio APIs and devices, open a
stream at a chosen sample rate and buffer size, and start/stop/close it.

Pairs naturally with `tur-rtmidi` for "audio + MIDI" applications and with
`tur-wav` for file-based I/O. Use it for synths, effects boxes, recorders,
and any real-time audio program.

## Install

```turmeric no-check
:spices {
  "rtaudio" {:url    "https://github.com/rjungemann/turmeric-spices"
             :ref    "rtaudio-v0.1.0"
             :subdir "spices/rtaudio"}
}
```

## Quick start

```turmeric
(import rtaudio/core    :refer [audio-new audio-free])
(import rtaudio/devices :refer [device-count device-info device-info-name])

(let [a (audio-new)]
  (let [n (device-count a)]
    (for [i (range 0 n)]
      (let [info (device-info a i)]
        (println i (device-info-name info)))))
  (audio-free a))
```

```sweet-exp
#lang sweet-exp
import rtaudio/core    :refer [audio-new audio-free]
import rtaudio/devices :refer [device-count device-info device-info-name]

let [a audio-new()]
  let [n device-count(a)]
    for [i range(0 n)]
      let [info device-info(a i)]
        println(i device-info-name(info))
  audio-free(a)
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/rtaudio>
