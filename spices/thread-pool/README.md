# tur-thread-pool

A bounded POSIX worker-thread pool for Turmeric. Workers pull `ptr<void>`
work items off a fixed-capacity ring buffer and hand each to a single
user-supplied callback. Producers get cooperative back-pressure: `pool-submit`
blocks while the queue is full rather than dropping work.

This is the generic core extracted from `tur-httpd`'s `httpd/pool`, which
previously hard-coded an `int` (socket fd) item type and a hard-coded HTTP
worker dispatch. `httpd/pool` is now a thin adapter over this spice.

## API (`thread-pool/pool`)

| Function | Signature | Description |
|---|---|---|
| `pool-new`    | `[size : int callback : ptr<void>] : ptr<void>` | Allocate the pool and spawn `size` workers (clamped to `>= 1`). `callback` is a `void (*)(void *item)` function pointer. Returns `NULL` on failure. |
| `pool-submit` | `[pool : ptr<void> item : ptr<void>] : void`    | Enqueue a work item; blocks when the ring buffer is full. Ownership of `item` transfers to the pool. |
| `pool-stop`   | `[pool : ptr<void>] : void`                     | Signal shutdown, let workers drain the queue, join all workers, free the pool. |

### Capacity

The ring buffer holds `max(size * 4, 64)` items, matching the original
`httpd/pool` formula (4× worker headroom, floor of 64).

### Ownership and lifetimes

- **Items.** Allocate a work item before `pool-submit`; the callback that runs
  on a worker thread is responsible for freeing it. The generic pool never
  inspects or frees item memory itself.
- **Callback.** Must remain valid for the full lifetime of the pool (same
  contract as the HTTP `handler` in the original design).
- **Shutdown.** Workers drain every successfully-submitted item before
  exiting, so each enqueued item is delivered to the callback exactly once.
  Do not call `pool-submit` after `pool-stop` has begun — join the producer
  thread first.

## Usage

A callback is a plain `void (*)(void *item)`. To obtain its address as a
`ptr<void>`, expose a tiny inline-C accessor:

```turmeric
(defmodule example
  (import thread-pool/pool :as tp)

(defn work-cb [item : ptr<void>] : void
  ```c
  int *v = (int *)item;
  /* ... do work ... */
  free(v);
  ```)

(defn work-cb-ptr [] : ptr<void>
  ```c
  extern void example__work_hycb(void *);
  return (void *)example__work_hycb;
  ```)

(defn main [] : int
  (let [pool (tp/pool-new 4 (work-cb-ptr))]
    ;; ... (tp/pool-submit pool item) ...
    (tp/pool-stop pool)
    0)))
```

## Tests

```sh
cd spices/thread-pool
tur test tests/thread-pool
```

Covers basic submit + drain, back-pressure (more items than ring capacity),
`pool-new` size clamping, stop-before-submit, and stop-with-queue-non-empty.

## Scope

In scope for v0.1.0: a single bounded FIFO queue with one callback. Out of
scope: priority lanes, dynamic resize, work-stealing, per-task futures, and
async-I/O integration.
