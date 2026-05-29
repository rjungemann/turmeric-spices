# tur-watch

Cross-platform filesystem watcher (inotify on Linux, kqueue on Darwin)
with debounce and coalescing helpers. Designed for CLI tools that need a
"file changed, react" loop without each tool re-inventing a watch loop.

> **Status:** v0.1.0 -- core spice (WT1-WT5 from the spice plan).
> Recursive directory watch (WT6), notebook cutover (WT7), and the
> `tur repl --watch` adopter (WT9) are tracked separately and not
> shipped here.

See `docs/notebook-watch-semantics.md` for the contract this spice must
preserve when notebook is cut over to depend on it.

## Modules

| Module | What's in it |
|---|---|
| `watch/event` | `watch-event` record, `watch-kind-*` constants, accessors |
| `watch/opts`  | `watch-opts` record, `default-watch-opts` (150 ms debounce) |
| `watch/watch` | `watch-open-one`, `watch-close`, `watch-next`, `watch-drain` |
| `watch/debounce` | `debounce-batch-*` primitive for path coalescing |

`watch/backend` is internal (the inotify+kqueue inline-C bridge) and is
not exported. Public callers should never import it.

## Quick start

```turmeric
(import watch/event :refer [watch-event-kind watch-event-free
                             watch-kind-write watch-kind-rename])
(import watch/opts  :refer [default-watch-opts])
(import watch/watch :refer [watch-open-one watch-close watch-next])

(defn main [] :int
  (let [opts (default-watch-opts)
        w    (watch-open-one "notes.md" opts)]
    (do
      (loop []
        (let [ev (watch-next w -1)]   ; block forever
          (if (= ev 0)
            0
            (do
              (println "file changed -- rerender now")
              (watch-event-free ev)
              (recur)))))
      (watch-close w))))
```

## Event kinds

| Kind | Numeric | When it fires |
|---|---|---|
| `watch-kind-write`    | 1 | file contents changed (Linux only; see notes) |
| `watch-kind-create`   | 2 | target appeared |
| `watch-kind-delete`   | 3 | target disappeared |
| `watch-kind-rename`   | 4 | inode changed -- typical for atomic-save editors |
| `watch-kind-attrib`   | 5 | metadata-only change |
| `watch-kind-overflow` | 6 | backend queue overflowed; fall back to full rebuild |

## Architecture (v0.1.0)

```
  client (notebook / repl / future build-watch)
                |
                v
          watch/watch              <-- stat-compare + classify + emit
                |
        +-------+--------+
        |                |
        v                v
   watch/debounce    watch/event
        |
        v
   watch/backend                   <-- platform inline-C, internal
        |
        v
  inotify  or  kqueue
```

The plan in the upstream Turmeric repo
(`docs/tur-watch-spice-plan.md`) describes the backend as two files
(`backend_linux.tur`, `backend_darwin.tur`). v0.1.0 ships them as a
single `watch/backend.tur` whose inline-C is `#ifdef`-guarded -- the
notebook code we extracted is already structured this way, and a
per-platform Turmeric-level split would just be two stubs of each other
on the wrong OS. Same outward contract.

## What v0.1.0 ships, vs. what it does not

| Capability | v0.1.0 | Deferred to |
|---|---|---|
| Single-file watch | yes | -- |
| Atomic-save (write temp + rename) on Linux + Darwin | yes | -- |
| In-place writes on Linux | yes | -- |
| In-place writes on Darwin | **no** (see semantics doc) | v0.2 |
| Bounded / forever / poll `watch-next` timeout | yes | -- |
| Burst-collapse via debounce window | yes | -- |
| Backend queue overflow surfaced explicitly | yes | -- |
| Multi-path `watch-open` | no (single path only) | WT6 |
| Recursive directory watch | no | WT6 |
| `watch-add-path` / `watch-remove-path` | stubs return -1 | WT6 |
| Callback / async API | no | v0.2 |
| macOS FSEvents backend | no | v0.2 |
| Windows backend | no | future |

## Testing

```sh
tur run tests/event_test.tur
tur run tests/opts_test.tur
tur run tests/backend_smoke_test.tur
tur run tests/debounce_test.tur
tur run tests/watch_test.tur          # spawns background pthreads
tur run tests/drain_burst_test.tur    # WT5 acceptance
```
