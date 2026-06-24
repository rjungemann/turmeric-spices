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

```turmeric no-check
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

### Linear `Db` / `Stmt` (U1)

`Db` and `Stmt` are `:linear` opaques. A connection from `ok-val` on
`db-open` must be closed exactly once with `db-close`, and a prepared
statement from `ok-val` on `db-prepare` must be finalized exactly once
with `stmt-finalize`. The query / step / bind / column operations take their
handle by `^borrow`, observing it without discharging that obligation.
Under `-Xsubstructural` this makes use-after-close, use-after-finalize,
double-close, and forgotten finalize into compile-time errors
(`TUR-E0101` / `TUR-E0100`) instead of runtime faults. The discipline is
inert in ordinary builds, so existing call sites compile unchanged. See
`tests/errors/` for the rejected cases. (db-query's transient statement
never escapes, so it stays a raw handle internally and needs no manual
finalize.)

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/sqlite>
