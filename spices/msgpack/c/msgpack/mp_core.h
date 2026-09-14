/* c/msgpack/mp_core.h -- MessagePack wire primitives for the msgpack spice.
 *
 * Hand-rolled and dependency-free: see the MP0 note in the spice README for
 * why this is not ludocode/mpack.  Two halves:
 *
 *   writer -- every mp_enc_* returns a fresh owned buffer laid out as
 *             { int64_t len; uint8_t data[]; }, the same shape as stdlib
 *             serial.tur's bytes value, so Buf and Bytes interoperate at the
 *             pointer level.  The caller owns it; release with mp_buf_free.
 *
 *   reader -- mp_tree_parse validates a whole buffer up front and copies it,
 *             so every offset the navigation functions hand back is known to
 *             be in bounds with a complete payload.  A node is a byte offset
 *             into that copy; -1 is "absent".
 *
 * Every reader entry point is total: a malformed or out-of-range argument
 * produces -1 / NULL / MP_T_INVALID, never a read past the end.
 */
#ifndef TUR_MSGPACK_MP_CORE_H
#define TUR_MSGPACK_MP_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- owned byte buffer: { int64_t len; uint8_t data[] } ---------------- */

void   *mp_buf_alloc(int64_t n);          /* zeroed, len = n */
int64_t mp_buf_len(const void *b);
void   *mp_buf_data(void *b);
void    mp_buf_free(void *b);
void   *mp_buf_concat(void *a, void *b);  /* CONSUMES a and b */
int64_t mp_buf_byte(const void *b, int64_t i);   /* -1 out of range */
int     mp_buf_eq(const void *a, const void *b);
char   *mp_buf_to_hex(const void *b);     /* malloc'd lowercase hex */
void   *mp_buf_of_hex(const char *hex);   /* NULL on a bad digit / odd length */

/* ---- writer ------------------------------------------------------------ */

void *mp_enc_nil(void);
void *mp_enc_bool(int v);
void *mp_enc_int(int64_t v);              /* smallest width that fits */
void *mp_enc_double(double v);            /* always float64 (0xcb) */
void *mp_enc_str(const char *s);          /* NULL encodes as nil */
void *mp_enc_array_header(int64_t n);
void *mp_enc_map_header(int64_t n);

/* Cons-list plumbing for the derive macros.  Both CONSUME the fragments they
 * walk (each is a malloc'd Buf); map keys are static literals and are not. */
void *mp_arr_build(int64_t cons_list);    /* [frag, ...]            -> array */
void *mp_map_build(int64_t cons_list);    /* [key, frag, key, ...]  -> map   */
void *mp_tag(const char *name, void *inner); /* {name: inner}; consumes inner */

/* ---- reader ------------------------------------------------------------ */

enum {
    MP_T_INVALID = 0, MP_T_NIL, MP_T_BOOL, MP_T_INT, MP_T_FLOAT,
    MP_T_STR, MP_T_BIN, MP_T_ARRAY, MP_T_MAP, MP_T_EXT
};

typedef struct mp_tree mp_tree;

/* Copies `len` bytes and validates that they are exactly one complete
 * MessagePack value with no trailing bytes.  NULL if they are not. */
mp_tree *mp_tree_parse(const void *bytes, int64_t len);
void     mp_tree_free(mp_tree *t);
int64_t  mp_tree_root(const mp_tree *t);  /* 0, or -1 for a NULL tree */

int      mp_type(const mp_tree *t, int64_t off);
const char *mp_type_name(const mp_tree *t, int64_t off);  /* schema vocabulary */

int64_t  mp_map_size(const mp_tree *t, int64_t off);   /* -1 if not a map   */
int64_t  mp_map_get(const mp_tree *t, int64_t off, const char *key);
int64_t  mp_arr_size(const mp_tree *t, int64_t off);   /* -1 if not an array */
int64_t  mp_arr_get(const mp_tree *t, int64_t off, int64_t i);

int   mp_get_int(const mp_tree *t, int64_t off, int64_t *out); /* 0 / -1 / -2 overflow */
int   mp_get_bool(const mp_tree *t, int64_t off, int *out);
int   mp_get_double(const mp_tree *t, int64_t off, double *out); /* ints widen */
char *mp_get_str_copy(const mp_tree *t, int64_t off);  /* malloc'd; NULL if not str */

/* Single-return forms, for callers (the Turmeric bindings) that cannot pass
 * an out-parameter through an inline-C body. */
int64_t mp_int_status(const mp_tree *t, int64_t off);  /* 0 ok / -1 not int / -2 overflow */
int64_t mp_int_value(const mp_tree *t, int64_t off);   /* 0 unless status == 0 */
int64_t mp_bool_value(const mp_tree *t, int64_t off);  /* 0 / 1 */
double  mp_double_value(const mp_tree *t, int64_t off);/* 0.0 unless numeric */

/* Type predicates.  Every one is false for an out-of-range offset, which is
 * what a missing map key looks like. */
int mp_is_absent(const mp_tree *t, int64_t off);
int mp_is_nil(const mp_tree *t, int64_t off);
int mp_is_bool(const mp_tree *t, int64_t off);
int mp_is_int(const mp_tree *t, int64_t off);
int mp_is_float(const mp_tree *t, int64_t off);
int mp_is_num(const mp_tree *t, int64_t off);   /* int or float */
int mp_is_str(const mp_tree *t, int64_t off);
int mp_is_arr(const mp_tree *t, int64_t off);
int mp_is_map(const mp_tree *t, int64_t off);

/* ---- accumulating decode errors ({path, expected, got} triples) -------- */

void    *mp_errs_new(void);
void     mp_errs_push(void *e, const char *path, const char *expected, const char *got);
int64_t  mp_errs_count(const void *e);
const char *mp_errs_path(const void *e, int64_t i);
const char *mp_errs_expected(const void *e, int64_t i);
const char *mp_errs_got(const void *e, int64_t i);
void     mp_errs_free(void *e);

#ifdef __cplusplus
}
#endif
#endif /* TUR_MSGPACK_MP_CORE_H */
