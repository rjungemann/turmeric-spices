// pcg32.h -- PCG32 pseudo-random number generator (minimal, MIT-licensed).
//
// Original design by M.E. O'Neill (pcg-random.org).
// This condensed single-header form is derived from the minimal C implementation
// at https://github.com/imneme/pcg-c-basic (Apache-2.0 / MIT dual-licensed).
//
// Usage:
//   pcg32_t rng;
//   pcg32_srandom_r(&rng, seed, stream);   // seed = 0 => PCG_DEFAULT_SEED
//   uint32_t v = pcg32_random_r(&rng);     // one draw
//   double   f = pcg32_double_r(&rng);     // uniform [0, 1)

#ifndef PCG32_H
#define PCG32_H

#include <stdint.h>

#define PCG_DEFAULT_MULTIPLIER_64  UINT64_C(6364136223846793005)
#define PCG_DEFAULT_INCREMENT_64   UINT64_C(1442695040888963407)

typedef struct { uint64_t state; uint64_t inc; } pcg32_t;

static inline void pcg32_srandom_r(pcg32_t *rng, uint64_t seed, uint64_t stream) {
    rng->state = 0U;
    rng->inc   = (stream << 1u) | 1u;
    rng->state = rng->state * PCG_DEFAULT_MULTIPLIER_64 + rng->inc;
    rng->state += seed;
    rng->state = rng->state * PCG_DEFAULT_MULTIPLIER_64 + rng->inc;
}

static inline uint32_t pcg32_random_r(pcg32_t *rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * PCG_DEFAULT_MULTIPLIER_64 + rng->inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

static inline double pcg32_double_r(pcg32_t *rng) {
    // Generates a double in [0, 1) with 32 bits of randomness.
    return (double)pcg32_random_r(rng) * (1.0 / 4294967296.0);
}

static inline uint64_t pcg32_uint64_r(pcg32_t *rng) {
    uint64_t hi = pcg32_random_r(rng);
    uint64_t lo = pcg32_random_r(rng);
    return (hi << 32u) | lo;
}

#endif /* PCG32_H */
