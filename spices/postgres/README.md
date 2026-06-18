# tur-postgres

PostgreSQL client for Turmeric via libpq: connect, exec, parameterized
queries, prepared statements, transactions, and `LISTEN`/`NOTIFY`.

## Overview

`tur-postgres` is a `cmake-dep` spice that wraps libpq. It exposes the
familiar PostgreSQL workflow: open a connection with `db-connect`, run
queries with `db-query` / `db-query-params`, prepare statements via
`stmt-prepare`, and walk rows with `row-get` / `col-name`.

It also surfaces `LISTEN` / `NOTIFY` so you can build event-driven services
that react to row changes without polling.

## Install

```turmeric no-check
:spices {
  "postgres" {:url    "https://github.com/rjungemann/turmeric-spices"
              :ref    "postgres-v0.1.0"
              :subdir "spices/postgres"}
}
```

## Quick start

```turmeric
(import postgres/db  :refer [db-connect db-close db-query])
(import postgres/row :refer [row-get rows-free])

(let [r (db-connect "postgresql://localhost/app")]
  (when (ok? r)
    (let [db   (ok-val r)
          rows (ok-val (db-query db "SELECT id, name FROM users"))]
      (for [row rows]
        (println (row-get row "id") (row-get row "name")))
      (rows-free rows)
      (db-close db))))
```

```sweet-exp
#lang sweet-exp
import postgres/db  :refer [db-connect db-close db-query]
import postgres/row :refer [row-get rows-free]

let [r db-connect("postgresql://localhost/app")]
  when ok?(r)
    let [db   ok-val(r)
         rows ok-val(db-query(db "SELECT id, name FROM users"))]
      for [row rows]
        println(row-get(row "id") row-get(row "name"))
      rows-free(rows)
      db-close(db)
```

### Linear `Conn` / `Rows` (U1)

`Conn` and `Rows` are `:linear` opaques. A connection from `conn-of` (or
`ok-val`) must be closed exactly once with `db-close` (PQfinish), and a
result set from `rows-of` must be released exactly once with `rows-free`
(PQclear). The exec / query / transaction / prepared-statement / notify
operations and the row accessors take their handle by `^borrow`. Under
`-Xsubstructural` this makes use-after-close, use-after-free, double-close,
and leaked connections / result sets into compile-time errors (`TUR-E0101`
/ `TUR-E0100`) instead of runtime faults. The discipline is inert in
ordinary builds, so existing call sites compile unchanged. (Prepared
statements are referenced by name, so there is no statement handle to
track.) See `tests/errors/` for the rejected cases.

## See also

- [API reference](api/)
- Source: <https://github.com/rjungemann/turmeric-spices/tree/main/spices/postgres>
