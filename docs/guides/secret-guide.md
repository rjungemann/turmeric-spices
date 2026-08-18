---
title: Secrets and Crypto Hygiene
category: Security
description: Linear key material with tur-secret -- a wipe the optimizer cannot delete, an OS CSPRNG, constant-time comparison, and a Secret type whose destructor the compiler enforces
---

# tur-secret guide

`tur-secret` covers the handling of key material: getting it, holding it,
comparing it, and destroying it. This guide is in two halves -- how to use it,
then how it works and why it is built the way it is. The second half matters
more than usual here, because most of the value in a spice like this is in
choices you cannot see from the API.

Design plan:
[`docs/upcoming/secret-spice-plan.md`](https://github.com/rjungemann/turmeric/blob/main/docs/upcoming/secret-spice-plan.md)
in the turmeric repo.

## Why it exists

Turmeric's standard library has no security-grade primitives, and the gaps
are not obvious from the names:

| You reach for | You get | Consequence |
|---|---|---|
| `stdlib/random.tur` | `rand()` seeded `time(NULL)` | a few thousand possible token streams |
| `raw-memset` | plain `memset` | deleted by the optimizer on a dying buffer |
| `stdlib/digest.tur` | SHA-256, no HMAC | raw hashes invite length-extension misuse |
| `=` on two buffers | `memcmp` | timing reveals the shared-prefix length |

None of these are bugs. They are correct implementations of things that are
not what you want when an attacker is in the picture. This spice provides the
versions that are.

## Getting it

`secret` is a workspace member, so a sibling spice in this repo can import it
with no fetch step. To declare it explicitly:

```turmeric no-check
:spices #map{
  "secret" #map{:path "../secret"}
}
```

Two modules:

- `secret/hygiene` -- three primitives that work on any raw buffer.
- `secret/core` -- the linear `Secret` handle built on top of them.

Use `secret/hygiene` directly when you are dealing with someone else's buffer
(a decoded header, a socket read). Use `secret/core` when you own the
material and want the compiler to hold you to destroying it.

---

# Part 1 -- Usage

## The hygiene floor

```turmeric
(import secret/hygiene :refer [secure-wipe-ptr! crypto-random-bytes!
                               crypto-random-source ct-eq-ptr?])
```

```sweet-exp
import secret/hygiene :refer [secure-wipe-ptr! crypto-random-bytes!
                              crypto-random-source ct-eq-ptr?]
```

### Getting entropy

```turmeric no-check
(crypto-random-bytes! p n)   ;; : (Result int cstr)
(crypto-random-source)       ;; : cstr -- names the backend
```

`crypto-random-bytes!` fills `n` bytes at `p` from the OS CSPRNG and returns
the count on success. On failure it wipes the destination rather than leaving
it partly filled, so a caller who ignores the `Result` ends up with zeros --
obviously broken -- instead of a short, low-entropy "key" that works.

`crypto-random-source` reports which backend the build selected
(`arc4random_buf`, `getrandom`, `BCryptGenRandom`, `/dev/urandom`). It is
worth logging at startup: it is how you find out you are on a weaker path
than you assumed.

### Comparing secrets

```turmeric no-check
(ct-eq-ptr? a b n)   ;; : bool -- constant time in the buffers' contents
```

Use this for MACs, session tokens, password hashes, and anything else an
attacker submits guesses against. `memcmp` returns at the first differing
byte, so its runtime tells the attacker how long a prefix they got right,
which turns an infeasible search into a linear one.

The timing is independent of the buffers' *contents*, not their *lengths*. To
compare variable-length secrets, compare fixed-size digests of them instead.

### Destroying secrets

```turmeric no-check
(secure-wipe-ptr! p n)   ;; : void
```

Use this and never `raw-memset` for key material. See
[Why a wipe needs help](#why-a-wipe-needs-help) -- this is not a
micro-optimization, it is the difference between the bytes being gone and
still being there.

## The linear `Secret`

`Secret` is the part that has no equivalent in Go, Rust `zeroize`, or
libsodium: a value the **compiler** will not let you forget to destroy.

```turmeric
(import secret/core :refer [Secret secret-random! secret-wipe!
                            with-secret secret-len secret-eq?])
(import secret/hygiene :refer [ct-eq-ptr?])

(defn derive-and-check [supplied : ptr<void> n : int] : bool
  (let [r (secret-random! 32)]
    (if (ok? r)
      (let [k  (ok-val r)
            eq (with-secret k (fn [p : ptr<void> m : int] : int
                                (if (ct-eq-ptr? p supplied m) 1 0)))]
        (do
          (secret-wipe! k)
          (= eq 1)))
      false)))
```

```sweet-exp
import secret/core :refer [Secret secret-random! secret-wipe!
                           with-secret secret-len secret-eq?]
import secret/hygiene :refer [ct-eq-ptr?]

defn derive-and-check [supplied : ptr<void> n : int] : bool
  let [r secret-random!(32)]
    if ok?(r)
      let [k  ok-val(r)
           eq with-secret(k (fn [p : ptr<void> m : int] : int
                              (if (ct-eq-ptr? p supplied m) 1 0)))]
        do
          secret-wipe!(k)
          =(eq 1)
      false
```

Delete the `(secret-wipe! k)` line and the program stops compiling:

```
error [TUR-E0100]: linear value 'k' dropped without being consumed
```

### The lifecycle

A `Secret` has exactly one legal shape of life:

```
  secret-random!  --+
                    +--> [ live ] --+--> secret-wipe!   (consumes; exactly once)
  secret-of-bytes --+       ^       |
                            |       +--> TUR-E0100 if this path is missing
                            |
             borrowed by with-secret / secret-len /
             secret-locked? / secret-eq? -- any number
             of times, none of them consuming
```

Three mistakes are compile errors rather than runtime hazards:

| Mistake | Diagnostic |
|---|---|
| never wiping | `TUR-E0100` linear value dropped without being consumed |
| reading after wiping | `TUR-E0101` linear value used after being consumed |
| wiping twice | `TUR-E0101` linear value used after being consumed |

**This is on by default.** As of `tur` 0.35, `-Xsubstructural` is a no-op that
warns `TUR-W0050`. You do not have to opt in, and you cannot accidentally
build without it.

The observing operations take the secret by `^borrow`, which means they look
at it without discharging the obligation -- so a long function that reads a
key five times still owes exactly one `secret-wipe!`.

### Constructing from existing bytes

```turmeric no-check
(secret-of-bytes p n)   ;; : (Result Secret cstr) -- and wipes p
```

`secret-of-bytes` **wipes the caller's buffer**. That is the reason it exists
instead of a plain copy: material that arrived in an ordinary buffer lives in
two places until the plain copy is destroyed, and the caller who has just
handed ownership over is the one least likely to remember the copy is still
there. Handing ownership over should destroy your copy, so it does.

### Checking that locking worked

```turmeric no-check
(secret-locked? k)   ;; : bool
```

Page locking is fail-soft (see [Locking](#locking-and-fail-soft) below). If
your threat model actually requires the key to stay out of swap, ask.

---

# Part 2 -- Implementation

## Why a wipe needs help

`memset(p, 0, n)` on a buffer nothing reads afterwards is a *dead store*. The
C standard lets the compiler delete it, and compilers do. The buffer you
"cleared" still holds the key.

This spice's `-O2` fixture measures it rather than asserting it. It fills a
512-byte stack frame with 64 copies of a magic pattern, wipes it, returns,
then has another function claim a frame at the same depth and count surviving
copies:

```
$ bash tests/o2/run.sh
stack-residue probe (built with -O2):
  no wipe (positive ctl)   residue=64
  plain memset (control)   residue=64
  secure-wipe-ptr!         residue=0
```

Apple clang at `-O2` deleted the `memset` outright -- the control is
indistinguishable from not wiping at all. That is the failure mode, observed,
on the toolchain most readers of this guide are using.

### The ladder

`secure-wipe-ptr!` picks the best primitive the platform offers:

1. `SecureZeroMemory` -- Win32; documented never to be elided.
2. `explicit_bzero` -- glibc >= 2.25, OpenBSD, FreeBSD, NetBSD, DragonFly.
3. A `volatile` function pointer to `memset` -- everywhere else.

Then a compiler memory barrier (`__asm__ __volatile__("" ::: "memory")`) on
GCC/clang, so the wipe cannot be sunk past a later use.

**macOS lands on rung 3.** Apple's libc ships neither `explicit_bzero` nor an
unguarded `memset_s` (`memset_s` requires `__STDC_WANT_LIB_EXT1__` to be
defined before `<string.h>`, which an inline-C body cannot guarantee, because
includes are hoisted). Rung 3 is not a weak fallback -- it is what libsodium
uses for the same reason:

```c
static void *(*volatile vmemset)(void *, int, size_t) = memset;
vmemset(p, 0, len);
```

The `volatile` is on the **pointer**, not the pointee. The compiler must
reload it and cannot prove which function it points at, so the call has to be
emitted. The measured `residue=0` above is this rung doing its job.

## The CSPRNG ladder

`BCryptGenRandom` -> `arc4random_buf` -> `getrandom(2)` -> `/dev/urandom`.

`arc4random_buf` is preferred on macOS and the BSDs because it cannot fail and
never blocks. On Linux, `getrandom(2)` is tried via `syscall(SYS_getrandom, ...)`
with `EINTR` retried, and falls through to `/dev/urandom` on `ENOSYS` (pre-3.17
kernels), which also covers any platform that reaches the bottom of the ladder.

The short-read path matters: if `/dev/urandom` returns fewer bytes than asked,
the destination is wiped and an error returned. Partial entropy is worse than
none, because it looks like it worked.

## Constant-time comparison

```c
const volatile unsigned char *x = a, *y = b;
volatile unsigned char acc = 0;
for (size_t i = 0; i < n; i++) acc |= (unsigned char)(x[i] ^ y[i]);
return acc == 0;
```

The accumulator is `volatile` so the compiler cannot introduce an early exit
once `acc` is known non-zero -- which, left to itself, is an optimization it
would be entitled to make, and which would reintroduce exactly the leak this
function exists to remove.

## The allocation shape

One allocation holds metadata and payload together:

```c
struct __tur_secret { int64_t len; int64_t locked; unsigned char data[]; };
```

One `malloc`, one `free`, and the payload pointer is always a fixed offset
from the handle. `Secret` rides the int carrier as a pointer to this struct.
It is the same shape the zlib spice's `__gzbuf` uses.

`secret-wipe!` zeroes the payload **before** unlocking the pages, because
`munlock` makes them swappable again -- unlock first and the window between
unlock and wipe is exactly the thing locking was there to prevent.

## Locking and fail-soft

At construction the whole allocation is `mlock`ed (header included -- the
kernel rounds to page granularity, so a partial lock buys nothing) and marked
`MADV_DONTDUMP` where that exists.

Locking failure is **not** fatal. `RLIMIT_MEMLOCK` is 64 KiB by default on
many Linux distributions, and a constructor that refused to mint a key over an
ambient ulimit would push callers back to raw `malloc` buffers -- strictly
worse than a locked-if-possible one. So the result is recorded and exposed via
`secret-locked?`, and callers who genuinely require locking can check.

## Why the API returns `Result`, and where it is built

Every fallible entry point returns `(Result T cstr)` -- never a sentinel
`:int`, per the project's standing rule.

But note **where** those Results are constructed: entirely in Turmeric, with
`ok`/`err`, while the inline-C helpers deal only in raw pointers and bools.
That split is a workaround, not a style preference. An inline-C body returns
the *boxed* Result carrier, and the box-to-struct conversion is silently
dropped in any function the CPS transform touches -- which includes every
caller of a higher-order function like `with-secret`. The result is a wall of
C type errors with no `.tur` attribution. Reported as
[`cps-result-unbox-dropped.md`](https://github.com/rjungemann/turmeric/blob/main/docs/reported/cps-result-unbox-dropped.md).

So `secret-random!` reads:

```turmeric no-check
(defn secret-random! [n : int] #fx{Rand} : (Result Secret cstr)
  (if (< n 1)
    (err "secret-random!: length must be positive")
    (let [h (secret-alloc-raw n)]
      (if (= h 0)
        (err "secret-random!: allocation failed")
        (if (secret-fill-random-raw! h n)
          (ok (secret-of-raw h))
          (do (secret-free-raw! h)
              (err "secret-random!: OS CSPRNG failed")))))))
```

Note the error path calls `secret-free-raw!` -- the same teardown
`secret-wipe!` uses. A mint whose entropy fill failed releases its pages
instead of handing back a zero-filled buffer that would pass for a key.

## Naming: `-raw`, not `__`

The private helpers are `secret-alloc-raw`, `secret-free-raw!`,
`secret-data-raw` -- not the `__`-prefixed convention used elsewhere in
stdlib and the spices. Inline-C bodies call siblings through
`__TUR_CNAME_<name>__`, and a name that itself begins with `__` breaks that
macro's expansion, producing a call to a function that does not exist. See
[`tur-cname-macro-breaks-on-leading-underscores.md`](https://github.com/rjungemann/turmeric/blob/main/docs/reported/tur-cname-macro-breaks-on-leading-underscores.md).

Privacy comes from not exporting them, which is what actually enforces it.

## Testing methodology

The suite is in three parts, and the third is the interesting one.

- `tur test tests` -- 26 functional cases.
- `bash errors/run.sh` -- the three linear-`Secret` diagnostics. These are
  compile-*fail* fixtures, so they live outside `tests/` (`tur test` recurses
  and would try to build them as an ordinary suite). The runner asserts each
  is rejected with the right code, and was itself checked in both directions:
  handed a fixture that compiles clean, it reports `not ok` and exits 1.
- `bash tests/o2/run.sh` -- the suite rebuilt at `-O2`, plus the dead-store
  probe.

### Two drafts of the probe that passed while measuring nothing

Worth recounting, because the failure mode generalizes.

**Draft 1** called the writer and the scanner directly. Both inlined into
`main`, their arrays got *disjoint* slots in one big frame, and the scanner
never looked at the memory the writer used. Result: `residue=0` everywhere,
including the plain-`memset` control. A clean pass, measuring nothing. Fixed
by calling both through `volatile` function pointers, which forces real calls
at the same stack depth.

**Draft 2** still read the scan buffer as an ordinary uninitialized array.
Reading uninitialized memory yields `undef`, which clang at `-O2` is entitled
to fold the comparison against -- so it deleted the entire scan loop and
returned zero hits regardless of what was in the frame. Another clean pass,
still measuring nothing. Fixed by reading through a `volatile` pointer.

Both were caught by adding a **positive control**: a frame that is written and
never wiped, which *must* show residue if the probe works at all. The probe now
reports `PASS (probe blind)` rather than `PASS` when the positive control comes
back empty.

The general lesson: a security test that can only report "clean" is
indistinguishable from a test that is not running. Give it something it is
required to find.

## What this does not protect against

Stated plainly, because a security tool that overstates its scope is worse
than none:

- **Register and stack spills.** The C compiler owns register allocation.
  Copies of key material it made in registers or spill slots are not tracked
  and not wiped. This is the tier libsodium, Rust `zeroize`, and Zig
  `secureZero` occupy; Go's `runtime/secret` is above it because the Go
  runtime owns the stack. `secret-do` (phase S4) would be a heuristic scrub,
  not a fix.
- **Pointer escape from `with-secret`.** The borrow checker stops the
  `Secret` escaping, but cannot follow the raw `ptr<void>` handed to the
  callback. Stash that pointer and you have a dangling reference to freed key
  material.
- **Core dumps on macOS.** No `MADV_DONTDUMP` analog exists. Accepted gap.
- **The interpreter.** None of the memory guarantees hold under
  `tur --interpret`: turi runs inline-C definitions only via registered
  natives or a pattern-matching heuristic, and none of these bodies match;
  its values are process-lifetime by design, so there is no point at which
  "the secret is gone" can be promised. Write code that uses this spice in the
  REPL; do not hold real key material there.
- **Everything above the process.** Swap that was written before the `mlock`,
  a debugger attached to the process, a hypervisor, or a backup of the
  machine's RAM.

## Where it sits

| | erasure guarantee | static enforcement |
|---|---|---|
| Go `runtime/secret` | registers + goroutine stack, by the runtime | none |
| libsodium / `zeroize` | heap buffer, best-effort | none |
| Rust `zeroize` + `Drop` | heap buffer, best-effort | destructor runs, but forgetting to *use* the type is silent |
| **tur-secret** | heap buffer, best-effort | **forgetting to destroy is a compile error** |

The honest summary: runtime hygiene at the libsodium tier, with a static
guarantee none of the others express. Turmeric cannot make Go's promise, and
this spice does not claim to.
