# tur-sqlite

SQLite3 database bindings for Turmeric: open/close, exec, parameterized
queries, prepared statements, and row access.

## Overview

`tur-sqlite` is a Tier 2 spice (`cmake-dep` -- pulls in SQLite 3.47.2 via
`tur fetch`). It wraps the SQLite C API in a small Turmeric layer: `db-open`
returns a `result<db>`, `db-exec` runs a statement, and `db-query` materializes
rows you can iterate with `row-get`.

Use it for local persistence, application caches, or any embedded relational
store. Threading and transaction semantics match the underlying SQLite
defaults.

## Install

```turmeric
:spices {
  "sqlite" {:url    "https://github.com/rjungemann/turmeric-spices"
            :ref    "sqlite-v0.1.0"
            :subdir "spices/sqlite"}
}
```

## Quick start

```turmeric
(import sqlite/db  :refer [db-open db-close db-exec db-query])
(import sqlite/row :refer [row-get])

(let [db (ok-val (db-open "app.db"))]
  (db-exec db "CREATE TABLE IF NOT EXISTS kv (k TEXT, v TEXT)")
  (db-exec db "INSERT INTO kv VALUES ('hello', 'world')")
  (let [rows (ok-val (db-query db "SELECT * FROM kv"))]
    (for [row rows]
      (println (row-get row "k") (row-get row "v"))))
  (db-close db))
```

```sweet-exp
#lang sweet-exp
import sqlite/db  :refer [db-open db-close db-exec db-query]
import sqlite/row :refer [row-get]

let [db ok-val(db-open("app.db"))]
  db-exec(db "CREATE TABLE IF NOT EXISTS kv (k TEXT, v TEXT)")
  db-exec(db "INSERT INTO kv VALUES ('hello', 'world')")
  let [rows ok-val(db-query(db "SELECT * FROM kv"))]
    for [row rows]
      println(row-get(row "k") row-get(row "v"))
  db-close(db)
```

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/sqlite>
