# tur-tidal

A Tidal Cycles-inspired mini-notation parser and pattern library for Turmeric.
`tur-tidal` parses the compact mini-notation used by Tidal Cycles into an
in-memory pattern tree, evaluates that tree at any cycle number to produce
a flat list of timed events, and renders the result as a plain-text event
table, a raw sclang `Pbind.new(...)` expression, or a scscm `pbind` form
for use with the `tur-scscm` spice.

## Mini-notation reference

| Syntax        | Meaning                                                   |
|---------------|-----------------------------------------------------------|
| `bd sd cp`    | Sequence: three equal-duration steps per cycle            |
| `[bd sd] cp`  | Subsequence: bd and sd share the first half-cycle slot    |
| `<60 62 64>`  | Alternating: one value per cycle, advancing each cycle    |
| `bd*2`        | Repeat: bd twice within its slot                          |
| `bd/2`        | Slow: bd spans 2 cycles                                   |
| `bd(3,8)`     | Euclidean rhythm: 3 pulses distributed across 8 steps     |
| `bd@2`        | Weight: bd takes twice as much time in a sequence         |
| `~`           | Rest: empty slot (no event produced)                      |

Note names (`c4`, `d#3`, `bb5`) are resolved to MIDI numbers by `note->midi`.
Raw integers (`60`, `72`) pass through as MIDI directly.
Drum names (`bd`, `sd`, `cp`) pass through as string values in events and
are not converted to MIDI (their `note->midi` result is -1, so they are
filtered out of `render-sclang` / `render-pbind` MIDI arrays).

## Quick example

```turmeric
(let [r (parse-notation "c4 [d4 e4] f4")]
  (let [p (ok-val r)]
    (println (render-sclang p "piano"))
    (notation-free p)))
```

Output:

```
(Pbind.new([\instrument, "piano", \midinote, Pseq.new([60,62,64,65],inf), \dur, Pseq.new([0.250,0.125,0.125,0.250],inf)])).play;
```

## Modules

### tidal/event

Low-level event struct (onset, dur, value) and the `note->midi` parser.

- `event-new onset dur value` -- allocate an event
- `event-onset e` -- onset fraction (0.0 to 1.0)
- `event-dur e`   -- duration fraction (0.0 to 1.0)
- `event-value e` -- raw string value
- `event-free e`  -- free the event
- `note->midi value` -- parse note name or integer to MIDI number (-1 for rest)

### tidal/notation

Mini-notation parser and evaluator.

- `parse-notation text` -- parse text into a pattern handle; returns `ok(p)` or `err(msg)`
- `notation-free p`     -- free a pattern handle
- `pattern-events p cycle` -- evaluate pattern at cycle, returns cons list of events

### tidal/pattern

Higher-level combinators that wrap an existing pattern handle.

- `pattern-fast p n`         -- speed up by factor n
- `pattern-slow p n`         -- slow down by factor n
- `pattern-stack p1 p2`      -- overlay: events from both patterns per cycle
- `pattern-cat p1 p2`        -- concatenate: alternate p1/p2 each cycle
- `pattern-rev p`            -- reverse event order within a cycle
- `pattern-every n f p`      -- apply transformed pattern f every nth cycle
- `pattern-degrade p prob`   -- randomly drop events with probability prob
- `pattern-free p`           -- free a combinator pattern

### tidal/render

Render a pattern to text.

- `render-events p cycle`      -- plain-text table: one `onset  dur  value` line per event
- `render-sclang p instrument` -- sclang `Pbind.new(...).play;` string
- `render-pbind p instrument`  -- scscm `(. (pbind ...) play)` string

## Dependencies

- **tur-scscm** (optional) -- provides the scscm compiler used by `render-pbind` output.
  `tur-tidal` produces the scscm text but does not require `tur-scscm` to be loaded
  in order to render it. The `tur-scscm` dependency is marked `:optional true` in
  `build.tur`.

## Building

```sh
just build
just test
```

## License

MIT
