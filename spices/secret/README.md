# `tur-secret` -- linear key material and crypto hygiene

Turmeric ships no security-grade memory or randomness primitives.
`stdlib/random.tur` is libc `rand()` seeded from `time(NULL)`,
`stdlib/digest.tur` has SHA-256 but no HMAC and no constant-time compare, and
`raw-memset` lowers to a plain `memset` the C compiler is free to delete. This
spice fills that gap.

It is modelled on Go's `runtime/secret` (Go 1.26,
`GOEXPERIMENT=runtimesecret`) but deliberately does not copy its mechanism.
Go can promise register and stack erasure because the Go runtime owns
register allocation; Turmeric emits C and hands that to `cc`, so the same
promise would be a lie. What Turmeric has that Go does not is **substructural
types**: a `:linear` `Secret` makes "you must wipe your key material" a
compile-time error rather than a code-review note.

So the offering is: libsodium-tier runtime hygiene, plus a static guarantee
libsodium cannot express.

## Status

| Phase | Contents | State |
|-------|----------|-------|
| S1 | `secure-wipe-ptr!`, `crypto-random-bytes!`, `ct-eq-ptr?` | shipped |
| S2 | linear `Secret`, `mlock`, `with-secret` | shipped |
| S3 | HMAC-SHA256, HKDF-SHA256, constant-time codecs | planned |
| S4 | `secret-do` best-effort stack scrub | optional |

Full walkthrough, including the implementation rationale and the testing
methodology: [`docs/guides/secret-guide.md`](../../docs/guides/secret-guide.md).

Design plan: [`docs/upcoming/secret-spice-plan.md`](https://github.com/rjungemann/turmeric/blob/main/docs/upcoming/secret-spice-plan.md)
in the compiler repo.

## `secret/hygiene`

Three primitives, usable against any raw buffer -- no `Secret` handle needed.

```turmeric
(import secret/hygiene :refer [secure-wipe-ptr! crypto-random-bytes!
                               crypto-random-source ct-eq-ptr?])

(defn mint-token [buf : ptr<void>] : bool
  (let [r (crypto-random-bytes! buf 32)]
    (ok? r)))
```

### `secure-wipe-ptr! [p : ptr<void> n : int] : void`

Zeroes `n` bytes at `p` through a primitive the optimizer may not elide:
`SecureZeroMemory` (Win32), `explicit_bzero` (glibc >= 2.25 and the BSDs), or
a `volatile` function pointer to `memset` (everywhere else, macOS included),
followed by a compiler memory barrier.

**This is not a micro-optimization detail.** The spice's own `-O2` fixture
measures it: a plain `memset` on a dying stack buffer is deleted outright by
Apple clang at `-O2`, leaving all 64 copies of the test pattern in the frame,
while `secure-wipe-ptr!` leaves none. Run `tests/o2/run.sh` to reproduce.

### `crypto-random-bytes! [p : ptr<void> n : int] : (Result int cstr)`

Fills `n` bytes from the OS CSPRNG: `BCryptGenRandom` -> `arc4random_buf` ->
`getrandom(2)` -> `/dev/urandom`. `crypto-random-source` reports which backend
this build selected, so a deployment can assert it is not on a weaker path
than expected.

On failure the destination is wiped rather than left partially filled, so a
caller that ignores the `Result` gets zeros, never low-entropy bytes.

### `ct-eq-ptr? [a : ptr<void> b : ptr<void> n : int] : bool`

Compares `n` bytes in time independent of their content, by accumulating the
XOR of every byte pair through a `volatile` accumulator. Use it for MACs,
tokens, and password hashes; `memcmp` returns at the first differing byte and
leaks the shared-prefix length to anyone who can time it.

It hides content, not length. To compare variable-length secrets, compare
fixed-size digests of them.

## `secret/core` -- the linear `Secret`

```turmeric
(import secret/core :refer [Secret secret-random! secret-wipe!
                            with-secret secret-len secret-eq?])

(defn sign-request [body : ptr<void> n : int] : int
  (let [r (secret-random! 32)]
    (if (ok? r)
      (let [k   (ok-val r)
            mac (with-secret k hmac-into)]
        (do
          (secret-wipe! k)      ;; omit this line and the program will not compile
          mac))
      0)))
```

`Secret` is `defopaque ... :linear`. Every control path through a live
`Secret` must end in exactly one `secret-wipe!`; the observing operations
(`with-secret`, `secret-eq?`, `secret-len`, `secret-locked?`) take it by
`^borrow` and do not discharge that obligation. Three things are therefore
compile errors, not runtime hazards:

| Mistake | Diagnostic |
|---|---|
| never wiping a secret | `TUR-E0100` linear value dropped without being consumed |
| reading one after wiping | `TUR-E0101` linear value used after being consumed |
| wiping one twice | `TUR-E0101` linear value used after being consumed |

`errors/run.sh` asserts all three. **This discipline is on by default** as of
`tur` 0.35 -- `-Xsubstructural` is now a no-op that warns `TUR-W0050`. It is
not an opt-in mode you have to remember to switch on.

### Constructors

`secret-random! [n] : (Result Secret cstr)` mints `n` bytes from the OS
CSPRNG. A secret that could not be filled is released rather than returned,
so the error path never yields a zero-filled "key".

`secret-of-bytes [p n] : (Result Secret cstr)` copies `n` bytes in and
**wipes the caller's buffer**. That wipe is the reason the constructor exists
rather than a plain copy: material that arrived in an ordinary buffer lives
in two places until the plain copy is destroyed, and the caller who has just
handed ownership over is the one least likely to remember.

### Reaching the bytes

`with-secret [s f]` is the only sanctioned route to the payload; there is
deliberately no accessor that returns the raw pointer. Note the limit
honestly: the borrow checker stops the `Secret` escaping, but it cannot
follow the `ptr<void>` handed to `f`. A callback that stashes that pointer
somewhere outliving the call holds a dangling reference to freed key
material, and nothing here catches it.

### Locking

Pages are `mlock`ed at construction and marked `MADV_DONTDUMP` where that
exists. Locking is **fail-soft** -- `RLIMIT_MEMLOCK` is 64 KiB by default on
many Linux distributions, and refusing to mint a key over an ambient ulimit
would push callers back to raw buffers, which is strictly worse. Callers who
genuinely require locking ask `secret-locked?`.

## Platform coverage

| | wipe | CSPRNG | page locking | core-dump exclusion |
|---|---|---|---|---|
| Linux (glibc >= 2.25) | `explicit_bzero` | `getrandom(2)` | `mlock` | `MADV_DONTDUMP` |
| macOS | volatile `memset` | `arc4random_buf` | `mlock` | none available |
| BSD | `explicit_bzero` | `arc4random_buf` | `mlock` | varies |
| Windows | `SecureZeroMemory` | `BCryptGenRandom` | `VirtualLock` | untested |

macOS has no `MADV_DONTDUMP` analog; that gap is accepted, not worked around.
Windows is untested -- the ladders are written but no CI runner exercises
spices there yet.

## Guarantees hold on the compiled path only

None of this survives `tur --interpret`. turi executes inline-C definitions
only via registered natives or a pattern-matching heuristic, and none of these
bodies match; turi's values are process-lifetime by design, so there is no
point at which "the secret is gone" can be promised. Treat the REPL as a place
to *write* code that uses this spice, never as a place to *hold* real key
material.

## Tests

```sh
tur test tests          # functional suite (26 cases)
bash tests/o2/run.sh    # same suite at -O2, plus the dead-store probe
bash errors/run.sh      # the three linear-Secret diagnostics
```

`errors/` holds compile-*fail* fixtures, which is why they sit outside
`tests/` -- `tur test tests` recurses and would try to build them as an
ordinary suite. `errors/run.sh` asserts each is rejected with the right
diagnostic code, rather than leaving the fixtures sitting unverified.

`tests/o2/residue.tur` is worth reading before trusting it: it carries a
positive control (a frame that is never wiped) alongside the memset control,
and reports "probe blind" rather than "pass" when the measurement turns out
not to observe anything. An earlier version of it silently passed while
measuring nothing at all.
