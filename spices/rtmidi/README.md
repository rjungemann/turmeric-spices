# tur-rtmidi

Cross-platform MIDI I/O for Turmeric via RtMidi. Open input or output
ports, send note/control/program messages, and decode incoming bytes.

## Overview

`tur-rtmidi` is a `cmake-dep` spice that wraps RtMidi. It exposes MIDI input
and output as separate handle types, with port enumeration, callback-based
receive, and a small builder API for common message types (note on/off,
CC, program change, pitch bend, aftertouch, sysex).

Use it for hardware integration -- controllers, synths, DAW remotes -- and
pair with `tur-rtaudio` for a complete real-time setup.

## Install

```turmeric no-check
:spices {
  "rtmidi" {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "rtmidi-v0.2.0"
            :subdir "spices/rtmidi"}
}
```

## Quick start

`midi-in-new` / `midi-out-new` return a real `(Result MidiIn int)` /
`(Result MidiOut int)` -- read the handle out with stdlib `ok-val`:

```turmeric
(import result      :refer [ok? ok-val])
(import rtmidi/core :refer [midi-out-new midi-out-free])
(import rtmidi/out  :refer [midi-out-open midi-out-close midi-out-send])
(import rtmidi/msg  :refer [msg-note-on msg-bytes msg-len msg-free])

(let [r (midi-out-new ":core-midi")]
  (if (ok? r)
    (let [m   (ok-val r)
          msg (msg-note-on 0 60 100)]
      (midi-out-open m 0 "tur")
      (midi-out-send m (msg-bytes msg) (msg-len msg))
      (msg-free msg)
      (midi-out-close m)
      (midi-out-free m))
    (println "midi-out init failed")))
```

```sweet-exp
#lang sweet-exp
import result      :refer [ok? ok-val]
import rtmidi/core :refer [midi-out-new midi-out-free]
import rtmidi/out  :refer [midi-out-open midi-out-close midi-out-send]
import rtmidi/msg  :refer [msg-note-on msg-bytes msg-len msg-free]

let [r midi-out-new(":core-midi")]
  if ok?(r)
    let [m   ok-val(r)
         msg msg-note-on(0 60 100)]
      midi-out-open(m 0 "tur")
      midi-out-send(m msg-bytes(msg) msg-len(msg))
      msg-free(msg)
      midi-out-close(m)
      midi-out-free(m)
    println("midi-out init failed")
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/rtmidi>
