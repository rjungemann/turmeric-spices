# tur-frame -- Dataframe Guide

> Spice version 0.1.0 -- Apache Arrow-compatible columnar in-memory format.
> Audience: Turmeric users who want pandas/dplyr-style data manipulation
> (read CSV, select columns, filter rows, group + aggregate, join, reshape).
>
> All ops are eager and immutable: every call returns a new frame whose
> underlying columns are refcount-shared with the source. Pair with
> [`tur-sqlite`](https://github.com/rjungemann/turmeric-spices/tree/main/spices/sqlite) for SQL round-trips, with PyArrow /
> DuckDB / Polars via the [Arrow C Data Interface](https://arrow.apache.org/docs/format/CDataInterface.html),
> or hand cells to [`tur-plutovg`](plutovg-guide.md) for charts.

This guide walks the seven things you'll do most often:

1. [Build a frame from columns or from CSV](#1-building-a-frame)
2. [Select, drop, rename, and add columns](#2-selectingfilteringmutating)
3. [Sort and de-duplicate](#3-sorting-and-deduplicating)
4. [Group-by and aggregate](#4-group-by-and-aggregation)
5. [Join two frames](#5-joining-two-frames)
6. [Reshape long-form with `melt`](#6-reshape-melt)
7. [Hand a frame to Python / R / DuckDB via Arrow](#7-arrow-c-data-interface)

Each section is a self-contained snippet you can drop into a `defn main`.

---

## 0. Installing the spice

In your project's `build.tur`:

```turmeric
:spices #{
  "frame" #{:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "frame-v0.1.0"
            :subdir "spices/frame"}
}
```

Then:

```sh
tur fetch
```

No CMake dependency -- `tur-frame` is pure Turmeric. The Arrow C Data Interface
header (`arrow_c.h`) is vendored; no libarrow link.

---

## 1. Building a frame

### From CSV

```turmeric
(import frame/csv   :refer [read-csv-string])
(import frame/print :refer [print-frame])

(let [src "id,name,age\n1,Alice,30\n2,Bob,25\n3,Carol,40\n"
      f   (read-csv-string src 0 0 1 0 "")]
  (print-frame f))
```

Output:

```
| id | name  | age |
| int64 | utf8  | int64 |
|------|-------|-------|
| 1    | Alice | 30    |
| 2    | Bob   | 25    |
| 3    | Carol | 40    |
```

The `read-csv-string` signature is
`(read-csv-string src delim quote has-header infer-rows null-str)`. Pass
0 for `delim` / `quote` to get the defaults (`,` and `"`), and 0 for
`infer-rows` to scan the first 100 rows for type inference (order:
int64 -> float64 -> date32 -> timestamp -> bool -> utf8).

Use `read-csv path delim quote has-header infer-rows null-str` to read
from a file directly.

### From explicit columns

```turmeric
(import frame/type    :refer [type-int64 type-utf8])
(import frame/column  :refer [column-int64 column-utf8])
(import frame/schema  :refer [field schema])
(import frame/frame   :refer [frame])

(let [ids     (column-int64 (cons 1 (cons 2 (cons 3 0))) 0 0)
      names   (column-utf8  (cons (cast "Alice" :int)
                            (cons (cast "Bob"   :int)
                            (cons (cast "Carol" :int) 0))) 0 0)
      s       (schema (cons (field "id"   (type-int64) 0)
                      (cons (field "name" (type-utf8)  0) 0)))
      f       (frame s (cons ids (cons names 0)))]
  (print-frame f))
```

Each `column-*` constructor takes `(vs nullable validity)` -- a cons list of
values, a 0/1 nullable flag (informational), and a parallel validity list
(or 0 for "all valid").

### From a builder (incremental)

```turmeric
(import frame/column :refer [column-builder builder-append-int64
                              builder-append-null builder-finish])

(let [b (column-builder (type-int64) 0)]
  (builder-append-int64 b 10)
  (builder-append-int64 b 20)
  (builder-append-null  b)
  (builder-append-int64 b 40)
  (let [col (builder-finish b)]
    ;; col has length 4, null_count 1
    col))
```

Builders are the right choice when constructing values one at a time (CSV
parsing, streaming readers).

---

## 2. Selecting / filtering / mutating

```turmeric
(import frame/select :refer [select-cols drop-cols rename with-col])
(import frame/filter :refer [filter-mask])
(import frame/column :refer [column-bool])

(let [f       (read-csv-string "id,name,age\n1,Alice,30\n2,Bob,25\n3,Carol,40\n" 0 0 1 0 "")

      ;; Keep only id and name; result has 3 rows × 2 cols.
      proj    (select-cols f (cons (cast "id" :int)
                              (cons (cast "name" :int) 0)))

      ;; Drop age; equivalent to (select-cols f (cons "id" (cons "name" 0))).
      dropped (drop-cols f (cons (cast "age" :int) 0))

      ;; Rename a column.
      renamed (rename f "age" "years")

      ;; Filter rows by a boolean mask.
      mask    (column-bool (cons 1 (cons 0 (cons 1 0))) 0 0)
      kept    (filter-mask f mask)        ;; keeps Alice and Carol

      ;; Add or replace a column.
      flag    (column-int64 (cons 100 (cons 200 (cons 300 0))) 0 0)
      with-f  (with-col f "score" (type-int64) flag)]
  ...)
```

Note: the public function is `select-cols`, not `select` -- `select` is
already a Turmeric special form for channel select.

`with-col` appends when the name is fresh, or replaces in-place when it
matches an existing column.

---

## 3. Sorting and de-duplicating

```turmeric
(import frame/sort   :refer [arrange])
(import frame/filter :refer [distinct])

(let [f       (read-csv-string "g,v\nB,3\nA,1\nA,2\nB,1\nC,5\n" 0 0 1 0 "")

      ;; Sort by g ascending, then v descending.
      names   (cons (cast "g" :int) (cons (cast "v" :int) 0))
      dirs    (cons 0 (cons 1 0))   ;; 0 = asc, 1 = desc
      sorted  (arrange f names dirs)

      ;; Remove duplicate rows (pass nil for the key list to use all columns).
      dedup   (distinct f (cons (cast "g" :int) 0))]
  ...)
```

`arrange` is stable -- rows that compare equal on every key preserve their
relative order from the input. Nulls sort low under ascending, high under
descending. The underlying engine is a single bottom-up merge sort that
handles int / float / bool / utf8 keys.

---

## 4. Group-by and aggregation

```turmeric
(import frame/group :refer [group-by grouped-count grouped-free
                             agg summarize
                             agg-count agg-sum agg-mean
                             agg-min agg-max agg-median agg-std])

(let [f      (read-csv-string "g,v\nA,1\nB,10\nA,2\nB,20\nA,3\n" 0 0 1 0 "")
      g      (group-by f (cons (cast "g" :int) 0))

      ;; Three parallel lists: output names, input names, aggregation tags.
      outs   (cons (cast "n"   :int)
             (cons (cast "sum" :int)
             (cons (cast "avg" :int) 0)))
      ins    (cons (cast "v" :int)
             (cons (cast "v" :int)
             (cons (cast "v" :int) 0)))
      tags   (cons (agg-count)
             (cons (agg-sum)
             (cons (agg-mean) 0)))

      result (agg g outs ins tags)]
  ;; result schema: g | n | sum | avg
  ;; rows: A 3 6 2.0; B 2 30 15.0
  (grouped-free g)
  ...)
```

Supported aggregation tags: `agg-count` `agg-sum` `agg-mean` `agg-min`
`agg-max` `agg-median` `agg-std` `agg-var`. Count outputs int64; mean /
median / std / var output float64; sum / min / max follow the input.

`summarize f outs ins tags` is a whole-frame variant -- equivalent to
`group-by` over a synthetic single-group then `agg`, producing a one-row
result frame.

---

## 5. Joining two frames

```turmeric
(import frame/join :refer [inner-join left-join right-join full-join
                            semi-join anti-join cross-join join])

(let [orders     (read-csv-string "uid,sku\n1,A\n1,B\n2,A\n3,C\n" 0 0 1 0 "")
      users      (read-csv-string "uid,name\n1,Alice\n2,Bob\n4,Dan\n" 0 0 1 0 "")
      keys       (cons (cast "uid" :int) 0)

      inner      (inner-join orders users keys keys)    ;; uid in BOTH (1, 2)
      left       (left-join  orders users keys keys)    ;; all orders, name=null for uid 3
      right      (right-join orders users keys keys)    ;; all users,  sku=null  for uid 4
      full       (full-join  orders users keys keys)    ;; union; nulls on either side

      with-orders (semi-join users orders keys keys)    ;; uids 1 & 2 only (no order rows)
      sans-orders (anti-join users orders keys keys)    ;; uid 4 only

      ;; Convenience form when keys have the same names on both sides:
      same       (join orders users "inner" keys)]
  ...)
```

Output schema: every column from the left frame, then every non-key
column from the right. If a right column collides with a left name, it
gets a `_r` suffix.

Engine: one hash-join driver shared by all six keyed variants (FNV-1a
row hashes + chained buckets). Build side is selected per join direction
(right for inner/left/semi/anti, left for right, both passes for full).

`cross-join l r` produces n_l × n_r rows; no keys required.

---

## 6. Reshape: melt

```turmeric
(import frame/reshape :refer [melt])

(let [wide (read-csv-string "id,group,x,y\n1,A,10,100\n2,B,20,200\n" 0 0 1 0 "")
      ids  (cons (cast "id" :int) (cons (cast "group" :int) 0))
      long (melt wide ids "var" "val")]
  ;; long has 4 rows × 4 cols:
  ;;   id group var val
  ;;   1  A     x   10
  ;;   2  B     x   20
  ;;   1  A     y   100
  ;;   2  B     y   200
  ...)
```

Outer loop is over non-id columns, inner over original rows (pandas
convention). All non-id columns must share a single type; `melt` returns
`0` otherwise.

`pivot` (long → wide) and `transpose` are intentionally **not** in
v0.1.0. The recommended path:

- For `pivot`, do `group-by` + `agg` first to collapse any duplicate
  `(index, key)` tuples, then reshape externally (PyArrow, DuckDB,
  Polars) after an `arrow-export` hop. `pivot-agg` may return in a
  later release once the duplicate-key reduction policy is settled.
- For `transpose`, do it in the receiving runtime after `arrow-export`
  -- PyArrow/Polars/DuckDB all have native transposes with better
  ergonomics than what a typed columnar layout can offer here.

See `docs/frame-spice-plan.md` "Potential later enhancements" for the
full rationale.

---

## 7. Arrow C Data Interface

Hand a frame to any consumer that speaks Arrow's [C Data Interface](https://arrow.apache.org/docs/format/CDataInterface.html):
PyArrow, R (nanoarrow), DuckDB (arrow_scan), Polars, etc.

```turmeric
(import frame/interop :refer [arrow-export arrow-import])

(let [f    (read-csv-string "x,y\n1,1.5\n2,2.5\n" 0 0 1 0 "")
      pair (arrow-export f)
      ;; pair is (schema-ptr . (array-ptr . nil))
      schema-ptr (cons-first  pair)
      array-ptr  (cons-second pair)]
  ;; Pass schema-ptr and array-ptr across an FFI boundary to the consumer.
  ;; Consumer calls schema->release(schema) and array->release(array) when done.
  ...)
```

v0 deep-copies on both export and import -- the buffers we hand out are
fresh allocations, and on import we copy into our standard aligned layout
and immediately invoke the consumer's release callbacks. Future v0.x will
add a zero-copy path that refcount-shares the underlying buffers.

### Consumer call patterns

**PyArrow** (Python):
```python
import pyarrow as pa
arr = pa.Array._import_from_c(int(array_ptr), int(schema_ptr))
# or pa.StructArray for a frame:
sa = pa.StructArray._import_from_c(int(array_ptr), int(schema_ptr))
# Convert to a Table:
table = pa.Table.from_struct_array(sa)
```

**R** with [nanoarrow](https://arrow.apache.org/nanoarrow/):
```r
library(nanoarrow)
arr <- as_nanoarrow_array_stream(list(schema = schema_ptr, array = array_ptr))
df  <- as.data.frame(arr)
```

**DuckDB** (in any host with DuckDB embedded):
```sql
CREATE TABLE t AS SELECT * FROM arrow_scan(?, ?);
-- bind two pointer parameters: array_ptr, schema_ptr
```

**Polars** (Python):
```python
import polars as pl
df = pl.from_arrow(pa.StructArray._import_from_c(int(array_ptr), int(schema_ptr)))
```

### Column-level export

```turmeric
(import frame/interop :refer [arrow-export-column arrow-import-column])

(let [col       (frame-column f "x")
      pair      (arrow-export-column col "x")
      ;; pass (schema-ptr, array-ptr) to the consumer
      ...
      imported  (arrow-import-column schema-ptr array-ptr)]
  ...)
```

---

## Reference: column types

| Tag | Name       | Arrow fmt | Storage                                                       |
|-----|------------|-----------|---------------------------------------------------------------|
| 1   | int32      | `i`       | 4 bytes per row                                               |
| 2   | int64      | `l`       | 8 bytes per row                                               |
| 3   | float32    | `f`       | 4 bytes per row                                               |
| 4   | float64    | `g`       | 8 bytes per row                                               |
| 5   | bool       | `b`       | 1 bit per row, packed                                         |
| 6   | utf8       | `u`       | int32 offsets + data buffer                                   |
| 7   | date32     | `tdD`     | int32 days-since-epoch                                        |
| 8   | timestamp  | `tsu:`    | int64 microseconds-since-epoch (UTC)                          |
| 9   | null       | `n`       | all-null sentinel                                             |

Columns are 64-byte aligned (Arrow spec; matches AVX-512 vector width and
typical cache line size).

---

## Integration with other spices

- **`tur-sqlite` / `tur-postgres`**: round-trip query results through a
  frame. Each driver can grow a tiny `frame-from-rows` helper in its own
  module; no dep added to `tur-frame` itself.

- **`tur-plutovg`**: build a scatter / histogram / line plot directly from
  a frame's columns -- the rendering layer just reads numeric cells via
  `column-float64-at` and `column-int64-at`.

- **`tur-json`**: pair with `read-jsonl` / `write-jsonl` for newline-delimited
  JSON interchange. A future `tur-frame-json` sub-spice could absorb this
  pattern.

---

## What's next (v0.2 candidates)

- **`pivot-agg`** -- long → wide with an explicit reduction
  (`agg-sum` / `agg-mean` / `agg-first`) for duplicate `(index, key)`
  combos. The "pivot then error on duplicates" version that lived
  briefly under FR7.5a was removed because the duplicate-key policy
  and the column-name stringification rules aren't worth locking in
  yet.
- **Pretty rendering of date32 / timestamp** in `print-frame`
  (currently shows the underlying int).
- **`tur-frame-parquet`** -- read/write Parquet (requires a Parquet C lib).
- **`tur-frame-lazy`** -- query-plan layer for chained operations.
- **Zero-copy Arrow interop** (FR8.1) -- share buffers via refcount and
  fire release callbacks at column-free time.
- **Date32 / timestamp arithmetic helpers** (`date+`, `date-days-between`,
  `timestamp->ymd`, etc.).

`transpose` is **not** on the v0.2 list -- the recommended path is
`arrow-export` and let PyArrow / Polars / DuckDB do it.
