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

```turmeric
:spices {
  "rtmidi" {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "rtmidi-v0.1.0"
            :subdir "spices/rtmidi"}
}
```

## Quick start

```turmeric
(import rtmidi/core :refer [midi-out-new midi-out-free midi-out-port-count])
(import rtmidi/out  :refer [midi-out-open midi-out-close midi-out-send])
(import rtmidi/msg  :refer [msg-note-on msg-free])

(let [m   (midi-out-new "tur")
      msg (msg-note-on 0 60 100)]
  (midi-out-open m 0)
  (midi-out-send m msg)
  (msg-free msg)
  (midi-out-close m)
  (midi-out-free m))
```

```sweet-exp
#lang sweet-exp
import rtmidi/core :refer [midi-out-new midi-out-free midi-out-port-count]
import rtmidi/out  :refer [midi-out-open midi-out-close midi-out-send]
import rtmidi/msg  :refer [msg-note-on msg-free]

let [m   midi-out-new("tur")
     msg msg-note-on(0 60 100)]
  midi-out-open(m 0)
  midi-out-send(m msg)
  msg-free(msg)
  midi-out-close(m)
  midi-out-free(m)
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/rtmidi>
