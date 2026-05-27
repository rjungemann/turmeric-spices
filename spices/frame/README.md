# tur-frame

In-memory dataframe (Arrow-compatible columnar) for Turmeric.

## Overview

`tur-frame` is a Tier 1 spice (pure Turmeric, no C deps). It provides an
in-memory columnar dataframe implementation that follows the Apache Arrow
memory layout (validity bitmap + values + offsets + 64-byte alignment). This
makes frames compatible with the [Arrow C Data Interface](https://arrow.apache.org/docs/format/CDataInterface.html),
so they can be handed to PyArrow, nanoarrow, DuckDB, Polars, and other Arrow
consumers via the `frame/interop` module.

The spice exports modules for: type definitions, buffers, columns, schemas,
frame construction, selection, filtering, sorting, joining, grouping, reshaping,
CSV I/O, printing, and Arrow interop.

## Install

```turmeric no-check
:spices {
  "frame" {:url    "https://github.com/rjungemann/turmeric-spices"
           :ref    "frame-v0.1.0"
           :subdir "spices/frame"}
}
```

## Quick start

```turmeric
(import frame/csv    :refer [read-csv-string])
(import frame/select :refer [select-cols])
(import frame/filter :refer [filter-mask])
(import frame/group  :refer [group-by agg agg-sum])
(import frame/print  :refer [print-frame])

(let [f       (read-csv-string "g,v\nA,10\nB,20\nA,30\n" 0 0 1 0 "")
      g       (group-by f (cons "g" 0))
      outs    (cons "total" 0)
      ins     (cons "v" 0)
      tags    (cons (agg-sum) 0)
      summary (agg g outs ins tags)]
  (print-frame summary))
```

```sweet-exp
#lang sweet-exp
import frame/csv    :refer [read-csv-string]
import frame/select :refer [select-cols]
import frame/filter :refer [filter-mask]
import frame/group  :refer [group-by agg agg-sum]
import frame/print  :refer [print-frame]

let [f       read-csv-string("g,v\nA,10\nB,20\nA,30\n" 0 0 1 0 "")
     g       group-by(f cons("g" 0))
     outs    cons("total" 0)
     ins     cons("v" 0)
     tags    cons(agg-sum() 0)
     summary agg(g outs ins tags)]
  print-frame(summary)
```

## See also

- [Guide](docs/guides/frame-guide.md)
- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/frame>
