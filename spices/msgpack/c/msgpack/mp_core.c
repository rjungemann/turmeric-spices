/* c/msgpack/mp_core.c -- MessagePack wire primitives.  See mp_core.h. */

#include "mp_core.h"

#include <stdlib.h>
#include <string.h>

/* ---- big-endian scalar helpers ---------------------------------------- */

static void be16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}
static void be64(uint8_t *p, uint64_t v) {
    be32(p, (uint32_t)(v >> 32));
    be32(p + 4, (uint32_t)v);
}
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}
static uint64_t rd64(const uint8_t *p) {
    return ((uint64_t)rd32(p) << 32) | (uint64_t)rd32(p + 4);
}

/* ---- owned byte buffer ------------------------------------------------- */

static void *buf_new(int64_t n, uint8_t **out_data) {
    if (n < 0) n = 0;
    int64_t *p = (int64_t *)malloc(sizeof(int64_t) + (size_t)n);
    if (!p) return NULL;
    p[0] = n;
    if (out_data) *out_data = (uint8_t *)(p + 1);
    return (void *)p;
}

void *mp_buf_alloc(int64_t n) {
    uint8_t *d;
    void *b = buf_new(n, &d);
    if (b && n > 0) memset(d, 0, (size_t)n);
    return b;
}

int64_t mp_buf_len(const void *b) { return b ? ((const int64_t *)b)[0] : 0; }
void   *mp_buf_data(void *b)      { return b ? (void *)((int64_t *)b + 1) : NULL; }
void    mp_buf_free(void *b)      { free(b); }

void *mp_buf_concat(void *a, void *b) {
    int64_t la = mp_buf_len(a), lb = mp_buf_len(b);
    uint8_t *d;
    void *out = buf_new(la + lb, &d);
    if (out) {
        if (la) memcpy(d, mp_buf_data(a), (size_t)la);
        if (lb) memcpy(d + la, mp_buf_data(b), (size_t)lb);
    }
    free(a);
    free(b);
    return out;
}

int64_t mp_buf_byte(const void *b, int64_t i) {
    if (!b || i < 0 || i >= mp_buf_len(b)) return -1;
    return (int64_t)((const uint8_t *)((const int64_t *)b + 1))[i];
}

int mp_buf_eq(const void *a, const void *b) {
    int64_t la = mp_buf_len(a), lb = mp_buf_len(b);
    if (la != lb) return 0;
    if (la == 0) return 1;
    return memcmp((const int64_t *)a + 1, (const int64_t *)b + 1, (size_t)la) == 0;
}

char *mp_buf_to_hex(const void *b) {
    static const char hex[] = "0123456789abcdef";
    int64_t n = mp_buf_len(b);
    char *out = (char *)malloc((size_t)(n * 2 + 1));
    if (!out) return NULL;
    const uint8_t *d = (const uint8_t *)((const int64_t *)b + 1);
    for (int64_t i = 0; i < n; i++) {
        out[i * 2]     = hex[d[i] >> 4];
        out[i * 2 + 1] = hex[d[i] & 0x0f];
    }
    out[n * 2] = '\0';
    return out;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void *mp_buf_of_hex(const char *hex) {
    if (!hex) return NULL;
    size_t n = strlen(hex);
    if (n % 2 != 0) return NULL;
    uint8_t *d;
    void *b = buf_new((int64_t)(n / 2), &d);
    if (!b) return NULL;
    for (size_t i = 0; i < n; i += 2) {
        int hi = hex_digit(hex[i]), lo = hex_digit(hex[i + 1]);
        if (hi < 0 || lo < 0) { free(b); return NULL; }
        d[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return b;
}

/* ---- writer ------------------------------------------------------------ */

void *mp_enc_nil(void) {
    uint8_t *d; void *o = buf_new(1, &d);
    if (o) d[0] = 0xc0;
    return o;
}

void *mp_enc_bool(int v) {
    uint8_t *d; void *o = buf_new(1, &d);
    if (o) d[0] = v ? 0xc3 : 0xc2;
    return o;
}

void *mp_enc_int(int64_t v) {
    uint8_t *d; void *o;
    if (v >= 0) {
        if (v <= 0x7f)             { o = buf_new(1, &d); if (o) { d[0] = (uint8_t)v; } }
        else if (v <= 0xffLL)      { o = buf_new(2, &d); if (o) { d[0] = 0xcc; d[1] = (uint8_t)v; } }
        else if (v <= 0xffffLL)    { o = buf_new(3, &d); if (o) { d[0] = 0xcd; be16(d + 1, (uint16_t)v); } }
        else if (v <= 0xffffffffLL){ o = buf_new(5, &d); if (o) { d[0] = 0xce; be32(d + 1, (uint32_t)v); } }
        else                       { o = buf_new(9, &d); if (o) { d[0] = 0xcf; be64(d + 1, (uint64_t)v); } }
    } else {
        if (v >= -32)              { o = buf_new(1, &d); if (o) { d[0] = (uint8_t)(int8_t)v; } }
        else if (v >= -128)        { o = buf_new(2, &d); if (o) { d[0] = 0xd0; d[1] = (uint8_t)(int8_t)v; } }
        else if (v >= -32768)      { o = buf_new(3, &d); if (o) { d[0] = 0xd1; be16(d + 1, (uint16_t)(int16_t)v); } }
        else if (v >= -2147483647LL - 1)
                                   { o = buf_new(5, &d); if (o) { d[0] = 0xd2; be32(d + 1, (uint32_t)(int32_t)v); } }
        else                       { o = buf_new(9, &d); if (o) { d[0] = 0xd3; be64(d + 1, (uint64_t)v); } }
    }
    return o;
}

void *mp_enc_double(double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    uint8_t *d; void *o = buf_new(9, &d);
    if (o) { d[0] = 0xcb; be64(d + 1, bits); }
    return o;
}

void *mp_enc_str(const char *s) {
    if (!s) return mp_enc_nil();
    size_t n = strlen(s);
    uint8_t *d; void *o;
    if (n <= 31) {
        o = buf_new((int64_t)(1 + n), &d);
        if (o) { d[0] = (uint8_t)(0xa0 | n); memcpy(d + 1, s, n); }
    } else if (n <= 0xff) {
        o = buf_new((int64_t)(2 + n), &d);
        if (o) { d[0] = 0xd9; d[1] = (uint8_t)n; memcpy(d + 2, s, n); }
    } else if (n <= 0xffff) {
        o = buf_new((int64_t)(3 + n), &d);
        if (o) { d[0] = 0xda; be16(d + 1, (uint16_t)n); memcpy(d + 3, s, n); }
    } else {
        o = buf_new((int64_t)(5 + n), &d);
        if (o) { d[0] = 0xdb; be32(d + 1, (uint32_t)n); memcpy(d + 5, s, n); }
    }
    return o;
}

static void *enc_hdr(int64_t n, uint8_t fixbase, uint8_t t16, uint8_t t32) {
    uint8_t *d; void *o;
    if (n < 0) n = 0;
    if (n <= 15)          { o = buf_new(1, &d); if (o) { d[0] = (uint8_t)(fixbase | (uint8_t)n); } }
    else if (n <= 0xffff) { o = buf_new(3, &d); if (o) { d[0] = t16; be16(d + 1, (uint16_t)n); } }
    else                  { o = buf_new(5, &d); if (o) { d[0] = t32; be32(d + 1, (uint32_t)n); } }
    return o;
}

void *mp_enc_array_header(int64_t n) { return enc_hdr(n, 0x90, 0xdc, 0xdd); }
void *mp_enc_map_header(int64_t n)   { return enc_hdr(n, 0x80, 0xde, 0xdf); }

/* Growable scratch used while concatenating fragment bodies. */
typedef struct { uint8_t *p; int64_t len, cap; } mp_sb;

static int sb_put(mp_sb *sb, const void *src, int64_t n) {
    if (n < 0) return 0;
    if (sb->len + n > sb->cap) {
        int64_t cap = sb->cap ? sb->cap * 2 : 64;
        while (cap < sb->len + n) cap *= 2;
        uint8_t *np = (uint8_t *)realloc(sb->p, (size_t)cap);
        if (!np) return 0;
        sb->p = np;
        sb->cap = cap;
    }
    if (n) memcpy(sb->p + sb->len, src, (size_t)n);
    sb->len += n;
    return 1;
}

/* Wrap a header plus an accumulated body into one owned buffer.  Consumes
 * both the header buffer and the scratch. */
static void *sb_finish(void *hdr, mp_sb *sb) {
    int64_t hn = mp_buf_len(hdr);
    uint8_t *d;
    void *out = buf_new(hn + sb->len, &d);
    if (out) {
        if (hn) memcpy(d, mp_buf_data(hdr), (size_t)hn);
        if (sb->len) memcpy(d + hn, sb->p, (size_t)sb->len);
    }
    free(hdr);
    free(sb->p);
    return out;
}

typedef struct mp_cons { int64_t head; int64_t tail; } mp_cons;

void *mp_arr_build(int64_t cons_list) {
    mp_sb sb = { NULL, 0, 0 };
    int64_t n = 0;
    for (mp_cons *q = (mp_cons *)(intptr_t)cons_list; q; q = (mp_cons *)(intptr_t)q->tail) {
        void *f = (void *)(intptr_t)q->head;
        if (f) {
            sb_put(&sb, mp_buf_data(f), mp_buf_len(f));
            free(f);
        } else {
            uint8_t nil_byte = 0xc0;
            sb_put(&sb, &nil_byte, 1);
        }
        n++;
    }
    return sb_finish(mp_enc_array_header(n), &sb);
}

void *mp_map_build(int64_t cons_list) {
    mp_sb sb = { NULL, 0, 0 };
    int64_t n = 0;
    mp_cons *q = (mp_cons *)(intptr_t)cons_list;
    while (q) {
        const char *k = (const char *)(intptr_t)q->head;
        q = (mp_cons *)(intptr_t)q->tail;
        if (!q) break;                       /* odd tail: drop the lone key */
        void *f = (void *)(intptr_t)q->head;
        q = (mp_cons *)(intptr_t)q->tail;

        void *kf = mp_enc_str(k);
        sb_put(&sb, mp_buf_data(kf), mp_buf_len(kf));
        free(kf);
        if (f) {
            sb_put(&sb, mp_buf_data(f), mp_buf_len(f));
            free(f);
        } else {
            uint8_t nil_byte = 0xc0;
            sb_put(&sb, &nil_byte, 1);
        }
        n++;
    }
    return sb_finish(mp_enc_map_header(n), &sb);
}

void *mp_tag(const char *name, void *inner) {
    mp_sb sb = { NULL, 0, 0 };
    void *kf = mp_enc_str(name);
    sb_put(&sb, mp_buf_data(kf), mp_buf_len(kf));
    free(kf);
    if (inner) {
        sb_put(&sb, mp_buf_data(inner), mp_buf_len(inner));
        free(inner);
    } else {
        uint8_t nil_byte = 0xc0;
        sb_put(&sb, &nil_byte, 1);
    }
    return sb_finish(mp_enc_map_header(1), &sb);
}

/* ---- reader ------------------------------------------------------------ */

struct mp_tree { uint8_t *data; int64_t len; };

/* Offset just past the complete element at `off`, or -1 if the bytes there
 * are truncated or not a legal encoding.  Iterative: `pending` counts the
 * elements still owed by the containers already opened, so a deeply nested
 * document costs no C stack. */
static int64_t mp_skip(const uint8_t *d, int64_t len, int64_t off) {
    static const int fixext_payload[5] = { 1, 2, 4, 8, 16 };
    int64_t pending = 1;
    while (pending > 0) {
        if (off < 0 || off >= len) return -1;
        uint8_t tag = d[off++];
        pending--;
        int64_t n;
        if (tag <= 0x7f || tag >= 0xe0) {
            /* fixint: the tag is the whole value */
        } else if (tag >= 0xa0 && tag <= 0xbf) {
            n = tag & 0x1f;
            if (off + n > len) return -1;
            off += n;
        } else if (tag >= 0x90 && tag <= 0x9f) {
            pending += (int64_t)(tag & 0x0f);
        } else if (tag >= 0x80 && tag <= 0x8f) {
            pending += 2 * (int64_t)(tag & 0x0f);
        } else switch (tag) {
            case 0xc0: case 0xc2: case 0xc3:
                break;
            case 0xcc: case 0xd0:
                if (off + 1 > len) return -1;
                off += 1;
                break;
            case 0xcd: case 0xd1:
                if (off + 2 > len) return -1;
                off += 2;
                break;
            case 0xca: case 0xce: case 0xd2:
                if (off + 4 > len) return -1;
                off += 4;
                break;
            case 0xcb: case 0xcf: case 0xd3:
                if (off + 8 > len) return -1;
                off += 8;
                break;
            case 0xc4: case 0xd9:
                if (off + 1 > len) return -1;
                n = d[off];
                off += 1 + n;
                if (off > len) return -1;
                break;
            case 0xc5: case 0xda:
                if (off + 2 > len) return -1;
                n = rd16(d + off);
                off += 2 + n;
                if (off > len) return -1;
                break;
            case 0xc6: case 0xdb:
                if (off + 4 > len) return -1;
                n = (int64_t)rd32(d + off);
                off += 4 + n;
                if (off > len) return -1;
                break;
            case 0xdc:
                if (off + 2 > len) return -1;
                pending += rd16(d + off);
                off += 2;
                break;
            case 0xdd:
                if (off + 4 > len) return -1;
                pending += (int64_t)rd32(d + off);
                off += 4;
                break;
            case 0xde:
                if (off + 2 > len) return -1;
                pending += 2 * (int64_t)rd16(d + off);
                off += 2;
                break;
            case 0xdf:
                if (off + 4 > len) return -1;
                pending += 2 * (int64_t)rd32(d + off);
                off += 4;
                break;
            case 0xd4: case 0xd5: case 0xd6: case 0xd7: case 0xd8:
                n = 1 + fixext_payload[tag - 0xd4];  /* type byte + payload */
                if (off + n > len) return -1;
                off += n;
                break;
            case 0xc7:
                if (off + 2 > len) return -1;
                n = d[off];
                off += 2 + n;
                if (off > len) return -1;
                break;
            case 0xc8:
                if (off + 3 > len) return -1;
                n = rd16(d + off);
                off += 3 + n;
                if (off > len) return -1;
                break;
            case 0xc9:
                if (off + 5 > len) return -1;
                n = (int64_t)rd32(d + off);
                off += 5 + n;
                if (off > len) return -1;
                break;
            default:            /* 0xc1 is never a legal type */
                return -1;
        }
    }
    return off;
}

mp_tree *mp_tree_parse(const void *bytes, int64_t len) {
    if (!bytes || len <= 0) return NULL;
    mp_tree *t = (mp_tree *)malloc(sizeof(*t));
    if (!t) return NULL;
    t->data = (uint8_t *)malloc((size_t)len);
    if (!t->data) { free(t); return NULL; }
    memcpy(t->data, bytes, (size_t)len);
    t->len = len;
    /* Exactly one complete value, no trailing bytes. */
    if (mp_skip(t->data, t->len, 0) != len) {
        free(t->data);
        free(t);
        return NULL;
    }
    return t;
}

void mp_tree_free(mp_tree *t) {
    if (!t) return;
    free(t->data);
    free(t);
}

int64_t mp_tree_root(const mp_tree *t) { return t ? 0 : -1; }

int mp_type(const mp_tree *t, int64_t off) {
    if (!t || off < 0 || off >= t->len) return MP_T_INVALID;
    uint8_t tag = t->data[off];
    if (tag <= 0x7f || tag >= 0xe0)          return MP_T_INT;
    if (tag >= 0xa0 && tag <= 0xbf)          return MP_T_STR;
    if (tag >= 0x90 && tag <= 0x9f)          return MP_T_ARRAY;
    if (tag >= 0x80 && tag <= 0x8f)          return MP_T_MAP;
    switch (tag) {
        case 0xc0: return MP_T_NIL;
        case 0xc2: case 0xc3: return MP_T_BOOL;
        case 0xc4: case 0xc5: case 0xc6: return MP_T_BIN;
        case 0xc7: case 0xc8: case 0xc9:
        case 0xd4: case 0xd5: case 0xd6: case 0xd7: case 0xd8: return MP_T_EXT;
        case 0xca: case 0xcb: return MP_T_FLOAT;
        case 0xcc: case 0xcd: case 0xce: case 0xcf:
        case 0xd0: case 0xd1: case 0xd2: case 0xd3: return MP_T_INT;
        case 0xd9: case 0xda: case 0xdb: return MP_T_STR;
        case 0xdc: case 0xdd: return MP_T_ARRAY;
        case 0xde: case 0xdf: return MP_T_MAP;
        default: return MP_T_INVALID;
    }
}

/* Vocabulary aligned with stdlib/schema.tur, with msgpack `map` reported as
 * "object" so a violation on the same struct reads identically to the json
 * spice's. */
const char *mp_type_name(const mp_tree *t, int64_t off) {
    switch (mp_type(t, off)) {
        case MP_T_NIL:   return "null";
        case MP_T_BOOL:  return "bool";
        case MP_T_INT:   return "int";
        case MP_T_FLOAT: return "float";
        case MP_T_STR:   return "string";
        case MP_T_BIN:   return "binary";
        case MP_T_ARRAY: return "array";
        case MP_T_MAP:   return "object";
        case MP_T_EXT:   return "ext";
        default:         return "missing";
    }
}

/* Offset of the first element of a container, or -1. */
static int64_t container_body(const mp_tree *t, int64_t off) {
    if (!t || off < 0 || off >= t->len) return -1;
    uint8_t tag = t->data[off];
    if ((tag >= 0x90 && tag <= 0x9f) || (tag >= 0x80 && tag <= 0x8f)) return off + 1;
    if (tag == 0xdc || tag == 0xde) return off + 3;
    if (tag == 0xdd || tag == 0xdf) return off + 5;
    return -1;
}

static int64_t container_size(const mp_tree *t, int64_t off, int want_map) {
    if (!t || off < 0 || off >= t->len) return -1;
    uint8_t tag = t->data[off];
    uint8_t fixlo = want_map ? 0x80 : 0x90, fixhi = want_map ? 0x8f : 0x9f;
    uint8_t t16   = want_map ? 0xde : 0xdc, t32   = want_map ? 0xdf : 0xdd;
    if (tag >= fixlo && tag <= fixhi) return tag & 0x0f;
    if (tag == t16) { if (off + 3 > t->len) return -1; return rd16(t->data + off + 1); }
    if (tag == t32) { if (off + 5 > t->len) return -1; return (int64_t)rd32(t->data + off + 1); }
    return -1;
}

int64_t mp_map_size(const mp_tree *t, int64_t off) { return container_size(t, off, 1); }
int64_t mp_arr_size(const mp_tree *t, int64_t off) { return container_size(t, off, 0); }

/* Pointer to a str value's bytes (never a bin), with its length. */
static const uint8_t *str_ptr(const mp_tree *t, int64_t off, int64_t *out_len) {
    if (mp_type(t, off) != MP_T_STR) return NULL;
    const uint8_t *d = t->data;
    uint8_t tag = d[off];
    int64_t n, hdr;
    if (tag >= 0xa0 && tag <= 0xbf) { n = tag & 0x1f;                 hdr = 1; }
    else if (tag == 0xd9)           { n = d[off + 1];                 hdr = 2; }
    else if (tag == 0xda)           { n = rd16(d + off + 1);          hdr = 3; }
    else                            { n = (int64_t)rd32(d + off + 1); hdr = 5; }
    if (off + hdr + n > t->len) return NULL;
    *out_len = n;
    return d + off + hdr;
}

int64_t mp_map_get(const mp_tree *t, int64_t off, const char *key) {
    if (!t || !key) return -1;
    int64_t n = mp_map_size(t, off);
    if (n < 0) return -1;
    int64_t p = container_body(t, off);
    if (p < 0) return -1;
    size_t klen = strlen(key);
    for (int64_t i = 0; i < n; i++) {
        int64_t voff = mp_skip(t->data, t->len, p);
        if (voff < 0) return -1;
        int64_t next = mp_skip(t->data, t->len, voff);
        if (next < 0) return -1;
        int64_t slen = 0;
        const uint8_t *s = str_ptr(t, p, &slen);
        if (s && (size_t)slen == klen && memcmp(s, key, klen) == 0) return voff;
        p = next;
    }
    return -1;
}

int64_t mp_arr_get(const mp_tree *t, int64_t off, int64_t i) {
    int64_t n = mp_arr_size(t, off);
    if (n < 0 || i < 0 || i >= n) return -1;
    int64_t p = container_body(t, off);
    if (p < 0) return -1;
    for (int64_t k = 0; k < i; k++) {
        p = mp_skip(t->data, t->len, p);
        if (p < 0) return -1;
    }
    return p;
}

/* Every offset reachable through the navigation API came out of a tree whose
 * whole buffer validated at parse time, so the payload bytes below are known
 * present.  The guard stays anyway: it is one compare, and it is the
 * difference between a bug here and a read past the end. */
static int payload_fits(const mp_tree *t, int64_t off, int64_t n) {
    return off >= 0 && n >= 0 && off + 1 + n <= t->len;
}

int mp_get_int(const mp_tree *t, int64_t off, int64_t *out) {
    if (mp_type(t, off) != MP_T_INT) return -1;
    const uint8_t *d = t->data;
    uint8_t tag = d[off];
    if (tag <= 0x7f) { *out = (int64_t)tag;         return 0; }
    if (tag >= 0xe0) { *out = (int64_t)(int8_t)tag; return 0; }
    switch (tag) {
        case 0xcc: if (!payload_fits(t, off, 1)) return -1; *out = (int64_t)d[off + 1]; return 0;
        case 0xcd: if (!payload_fits(t, off, 2)) return -1; *out = (int64_t)rd16(d + off + 1); return 0;
        case 0xce: if (!payload_fits(t, off, 4)) return -1; *out = (int64_t)rd32(d + off + 1); return 0;
        case 0xcf: {
            if (!payload_fits(t, off, 8)) return -1;
            uint64_t v = rd64(d + off + 1);
            if (v > (uint64_t)INT64_MAX) return -2;
            *out = (int64_t)v; return 0;
        }
        case 0xd0: if (!payload_fits(t, off, 1)) return -1; *out = (int64_t)(int8_t)d[off + 1]; return 0;
        case 0xd1: if (!payload_fits(t, off, 2)) return -1; *out = (int64_t)(int16_t)rd16(d + off + 1); return 0;
        case 0xd2: if (!payload_fits(t, off, 4)) return -1; *out = (int64_t)(int32_t)rd32(d + off + 1); return 0;
        case 0xd3: if (!payload_fits(t, off, 8)) return -1; *out = (int64_t)rd64(d + off + 1); return 0;
        default:   return -1;
    }
}

int mp_get_bool(const mp_tree *t, int64_t off, int *out) {
    if (mp_type(t, off) != MP_T_BOOL) return -1;
    *out = (t->data[off] == 0xc3) ? 1 : 0;
    return 0;
}

int mp_get_double(const mp_tree *t, int64_t off, double *out) {
    int k = mp_type(t, off);
    if (k == MP_T_INT) {
        int64_t v;
        int rc = mp_get_int(t, off, &v);
        if (rc != 0) return rc;
        *out = (double)v;
        return 0;
    }
    if (k != MP_T_FLOAT) return -1;
    const uint8_t *d = t->data;
    if (d[off] == 0xca) {
        if (!payload_fits(t, off, 4)) return -1;
        uint32_t bits = rd32(d + off + 1);
        float f;
        memcpy(&f, &bits, sizeof(f));
        *out = (double)f;
        return 0;
    }
    if (!payload_fits(t, off, 8)) return -1;
    uint64_t bits = rd64(d + off + 1);
    double dv;
    memcpy(&dv, &bits, sizeof(dv));
    *out = dv;
    return 0;
}

char *mp_get_str_copy(const mp_tree *t, int64_t off) {
    int64_t n = 0;
    const uint8_t *s = str_ptr(t, off, &n);
    if (!s) return NULL;
    char *out = (char *)malloc((size_t)n + 1);
    if (!out) return NULL;
    memcpy(out, s, (size_t)n);
    out[n] = '\0';
    return out;
}

int64_t mp_int_status(const mp_tree *t, int64_t off) {
    int64_t v;
    return (int64_t)mp_get_int(t, off, &v);
}

int64_t mp_int_value(const mp_tree *t, int64_t off) {
    int64_t v = 0;
    if (mp_get_int(t, off, &v) != 0) return 0;
    return v;
}

int64_t mp_bool_value(const mp_tree *t, int64_t off) {
    int b = 0;
    if (mp_get_bool(t, off, &b) != 0) return 0;
    return (int64_t)b;
}

double mp_double_value(const mp_tree *t, int64_t off) {
    double d = 0.0;
    if (mp_get_double(t, off, &d) != 0) return 0.0;
    return d;
}

int mp_is_absent(const mp_tree *t, int64_t off) { return mp_type(t, off) == MP_T_INVALID; }
int mp_is_nil(const mp_tree *t, int64_t off)    { return mp_type(t, off) == MP_T_NIL; }
int mp_is_bool(const mp_tree *t, int64_t off)   { return mp_type(t, off) == MP_T_BOOL; }
int mp_is_int(const mp_tree *t, int64_t off)    { return mp_type(t, off) == MP_T_INT; }
int mp_is_float(const mp_tree *t, int64_t off)  { return mp_type(t, off) == MP_T_FLOAT; }
int mp_is_str(const mp_tree *t, int64_t off)    { return mp_type(t, off) == MP_T_STR; }
int mp_is_arr(const mp_tree *t, int64_t off)    { return mp_type(t, off) == MP_T_ARRAY; }
int mp_is_map(const mp_tree *t, int64_t off)    { return mp_type(t, off) == MP_T_MAP; }

int mp_is_num(const mp_tree *t, int64_t off) {
    int k = mp_type(t, off);
    return k == MP_T_INT || k == MP_T_FLOAT;
}

/* ---- accumulating decode errors --------------------------------------- */

typedef struct { char *path, *expected, *got; } mp_err;
typedef struct { int64_t count, cap; mp_err *items; } mp_errs;

static char *dup_cstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

void *mp_errs_new(void) {
    mp_errs *e = (mp_errs *)malloc(sizeof(*e));
    if (!e) return NULL;
    e->count = 0; e->cap = 0; e->items = NULL;
    return e;
}

void mp_errs_push(void *ep, const char *path, const char *expected, const char *got) {
    mp_errs *e = (mp_errs *)ep;
    if (!e) return;
    if (e->count >= e->cap) {
        int64_t cap = e->cap ? e->cap * 2 : 4;
        mp_err *items = (mp_err *)realloc(e->items, (size_t)cap * sizeof(mp_err));
        if (!items) return;
        e->items = items;
        e->cap = cap;
    }
    e->items[e->count].path     = dup_cstr(path);
    e->items[e->count].expected = dup_cstr(expected);
    e->items[e->count].got      = dup_cstr(got);
    e->count++;
}

int64_t mp_errs_count(const void *ep) {
    const mp_errs *e = (const mp_errs *)ep;
    return e ? e->count : 0;
}

static const char *errs_field(const void *ep, int64_t i, int which) {
    const mp_errs *e = (const mp_errs *)ep;
    if (!e || i < 0 || i >= e->count) return "";
    const mp_err *it = &e->items[i];
    const char *s = (which == 0) ? it->path : (which == 1) ? it->expected : it->got;
    return s ? s : "";
}

const char *mp_errs_path(const void *e, int64_t i)     { return errs_field(e, i, 0); }
const char *mp_errs_expected(const void *e, int64_t i) { return errs_field(e, i, 1); }
const char *mp_errs_got(const void *e, int64_t i)      { return errs_field(e, i, 2); }

void mp_errs_free(void *ep) {
    mp_errs *e = (mp_errs *)ep;
    if (!e) return;
    for (int64_t i = 0; i < e->count; i++) {
        free(e->items[i].path);
        free(e->items[i].expected);
        free(e->items[i].got);
    }
    free(e->items);
    free(e);
}
