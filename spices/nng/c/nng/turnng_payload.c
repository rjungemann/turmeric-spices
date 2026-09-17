/* turnng_payload.c -- the owned byte buffer behind `nng/buf`.
 *
 * Layout is deliberately identical to the msgpack spice's `Buf` (and to
 * stdlib serial.tur's bytes value): one heap block holding an int64 length
 * followed by that many data bytes.
 *
 *     +----------------+------------------------+
 *     | int64_t len    | uint8_t data[len]      |
 *     +----------------+------------------------+
 *
 * Sharing the layout is what lets a msgpack-encoded payload cross into
 * `send-buf` without a second copy: the two handles are the same pointer
 * shape, so a caller can hand one to the other once the types are bridged.
 *
 * The `turnng_` prefix (rather than `nng_`) keeps these symbols out of
 * libnng's namespace -- this file is linked into the same binary as nng
 * itself, and `nng_buf_alloc` is exactly the kind of name a future nng
 * release could claim.
 *
 * Nothing here includes <nng/nng.h>: the vendored source is compiled on the
 * same command line as the emitted C, but keeping it dependency-free means a
 * buffer bug is never entangled with the dep's include path.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Allocate a block for `n` data bytes and hand back the data pointer via
 * `*out`. Returns NULL (leaving *out untouched) on overflow or OOM. */
static void *turnng_payload_new(int64_t n, uint8_t **out) {
    if (n < 0) return NULL;
    void *b = malloc(sizeof(int64_t) + (size_t)n);
    if (!b) return NULL;
    ((int64_t *)b)[0] = n;
    *out = (uint8_t *)((int64_t *)b + 1);
    return b;
}

void *turnng_payload_alloc(int64_t n) {
    uint8_t *d = NULL;
    void *b = turnng_payload_new(n, &d);
    if (b && n > 0) memset(d, 0, (size_t)n);
    return b;
}

int64_t turnng_payload_len(const void *b) { return b ? ((const int64_t *)b)[0] : 0; }
void   *turnng_payload_data(void *b)      { return b ? (void *)((int64_t *)b + 1) : NULL; }
void    turnng_payload_free(void *b)      { free(b); }

/* Copy `n` bytes from `src` into a fresh buffer. The one constructor the nng
 * receive path needs: nng hands back its own allocation, which must be
 * released with nng_free before the wrapper returns. */
void *turnng_payload_of_bytes(const void *src, int64_t n) {
    uint8_t *d = NULL;
    void *b = turnng_payload_new(n, &d);
    if (b && n > 0) memcpy(d, src, (size_t)n);
    return b;
}

int64_t turnng_payload_byte(const void *b, int64_t i) {
    if (!b || i < 0 || i >= turnng_payload_len(b)) return -1;
    return (int64_t)((const uint8_t *)((const int64_t *)b + 1))[i];
}

int turnng_payload_eq(const void *a, const void *b) {
    int64_t la = turnng_payload_len(a), lb = turnng_payload_len(b);
    if (la != lb) return 0;
    if (la == 0) return 1;
    return memcmp((const int64_t *)a + 1, (const int64_t *)b + 1, (size_t)la) == 0;
}

/* A NUL-terminated copy of the payload. Binary payloads with embedded NULs
 * truncate at the first one -- that is the documented limit of the cstr
 * conveniences, not a defect here. */
char *turnng_payload_to_cstr(const void *b) {
    int64_t n = turnng_payload_len(b);
    char *out = (char *)malloc((size_t)n + 1);
    if (!out) return NULL;
    if (n > 0) memcpy(out, (const int64_t *)b + 1, (size_t)n);
    out[n] = '\0';
    return out;
}

char *turnng_payload_to_hex(const void *b) {
    static const char hex[] = "0123456789abcdef";
    int64_t n = turnng_payload_len(b);
    const uint8_t *p = (const uint8_t *)((const int64_t *)b + 1);
    char *out = (char *)malloc((size_t)(n * 2 + 1));
    if (!out) return NULL;
    for (int64_t i = 0; i < n; i++) {
        out[i * 2]     = hex[(p[i] >> 4) & 0xf];
        out[i * 2 + 1] = hex[p[i] & 0xf];
    }
    out[n * 2] = '\0';
    return out;
}
