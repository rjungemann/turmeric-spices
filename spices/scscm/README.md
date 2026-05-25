# tur-scscm

An scscm (Scheme-like s-expression syntax) to sclang (SuperCollider language)
compiler written in Turmeric. Also includes an OSC client for sending note and
performance control messages to a running scsynth or hcsynth server (SC6, coming
soon).

## Overview

`tur-scscm` is a Tier 1 spice (pure Turmeric + inline-C, no cmake deps). The
compilation pipeline is:

```
scscm text
  -> [lexer]    token list       (scscm/lexer)
  -> [parser]   AST              (scscm/parser)
  -> [expander] expanded AST     (scscm/expander)
  -> [codegen]  sclang text      (scscm/codegen)
```

Use `scscm/compile` for the public one-shot API.

## Quick start

```turmeric
(load "stdlib/list.tur")
(load "stdlib/result.tur")
(load "spices/tur-scscm/src/scscm/lexer.tur")
(load "spices/tur-scscm/src/scscm/parser.tur")
(load "spices/tur-scscm/src/scscm/codegen.tur")
(load "spices/tur-scscm/src/scscm/expander.tur")
(load "spices/tur-scscm/src/scscm/compile.tur")

(let [res (compile-text "(+ 1 2)")]
  (if (ok? res)
    (println (scscm-int-as-cstr (ok-val res)))
    (println "error")))
;; => "1 + 2"
```

```sweet-exp
#lang sweet-exp
load("stdlib/list.tur")
load("stdlib/result.tur")
load("spices/tur-scscm/src/scscm/lexer.tur")
load("spices/tur-scscm/src/scscm/parser.tur")
load("spices/tur-scscm/src/scscm/codegen.tur")
load("spices/tur-scscm/src/scscm/expander.tur")
load("spices/tur-scscm/src/scscm/compile.tur")

let [res compile-text("(+ 1 2)")]
  if ok?(res)
    println $ scscm-int-as-cstr ok-val(res)
    println("error")
;; => "1 + 2"
```

## scscm language reference

### Data literals

| scscm | sclang |
|-------|--------|
| `nil` `true` `false` | `nil` `true` `false` |
| `440` `0.5` `-1` | `440` `0.5` `-1` |
| `"hello"` | `"hello"` |
| `:freq` | `\freq` |
| `foo-bar` | `foo_bar` |
| `done?` | `done_p` |

### Special forms

| scscm | sclang |
|-------|--------|
| `(fn [a b] body)` | `{ \|a, b\| body }` |
| `(defn name [a] body)` | `name = { \|a\| body }` |
| `(var x val)` | `var x = val` |
| `(set! x val)` | `x = val` |
| `(let [x v y w] body)` | `var x = v;\nvar y = w;\nbody` |
| `(if c t e)` | `if(c, { t }, { e })` |
| `(do e1 e2)` | `e1;\ne2` |
| `(. obj msg args...)` | `obj.msg(args...)` |
| `(.dot Cls msg args...)` | `Cls.msg(args...)` |

### Macro stdlib

| scscm | expands to |
|-------|-----------|
| `(-> x f g)` | thread-first: `g(f(x))` |
| `(->> x f g)` | thread-last: `g(f(x))` |
| `(when c body)` | `if(c, { body }, { nil })` |
| `(unless c body)` | `if(not(c), { body }, { nil })` |
| `(doseq [x seq] body)` | `seq.do { \|x\| body }` |
| `(dotimes [i n] body)` | `n.do { \|i\| body }` |
| `(collect [x seq] body)` | `seq.collect { \|x\| body }` |
| `(pbind :k v ...)` | `Pbind(\k, v, ...)` |
| `(pseq items n)` | `Pseq(items, n)` |
| `(prand items n)` | `Prand(items, n)` |
| `(pwhite lo hi n)` | `Pwhite(lo, hi, n)` |
| `(pgeom start step n)` | `Pgeom(start, step, n)` |
| `(defsynth name [p] body)` | `SynthDef.new(\name, { \|p\| body }).add` |
| `(adsr a d s r)` | `Env.adsr(a, d, s, r)` |
| `(perc atk rel)` | `Env.perc(atk, rel)` |
| `(env-line lvls times)` | `Env.new(lvls, times)` |
| `(midi->hz n)` | `n.midicps` |
| `(hz->midi n)` | `n.cpsmidi` |
| `(db->amp db)` | `db.dbamp` |
| `(amp->db amp)` | `amp.ampdb` |

### User-defined macros

```scscm
(defmacro my-inc [x]
  `(+ ~x 1))

(my-inc 5)   ;; => 6
```

Macros are hygienic substitution templates: `~x` is `unquote` (splice the
argument), `` `(...) `` is `quasiquote`.

## API

### scscm/lexer

```turmeric
(tokenize source :cstr)              ;; => result<tokens :int>
(token-type tok :int) :cstr          ;; => ":symbol" ":keyword" etc.
(token-value tok :int) :cstr
(token-line tok :int) :int
(token-col tok :int) :int
(tokens-free ts :int) :void
```

### scscm/parser

```turmeric
(parse tokens :int)                  ;; => result<ast-list :int>
(ast-kind node :int) :cstr           ;; => ":list" ":symbol" etc.
(ast-symbol-name node :int) :cstr
(ast-number-value node :int) :cstr
(ast-string-value node :int) :cstr
(ast-list-len node :int) :int
(ast-list-get node :int i :int) :int
(ast-free node :int) :void
(ast-free-all nodes :int) :void
```

### scscm/expander

```turmeric
(expand ast :int) :int               ;; expand one node
(expand-all asts :int) :int          ;; expand a cons list
```

### scscm/codegen

```turmeric
(generate ast :int) :cstr            ;; sclang text for one node
(generate-all asts :int) :cstr       ;; joined with ";\n"
```

### scscm/compile

```turmeric
(compile-text source :cstr)          ;; => result<sclang :cstr>
(compile-file path :cstr)            ;; => result<sclang :cstr>
```

## Implementation notes

- The lexer (`scscm/lexer`) uses a single inline-C block for character-level
  scanning. All higher-level logic is pure Turmeric.
- AST nodes are heap-allocated C structs passed as `:int` opaque handles,
  mirroring the pattern in `stdlib/list.tur`.
- The expander maintains a C-static global macro environment. Macros defined
  via `defmacro` persist for the lifetime of the process.
- The codegen uses `str-concat`-style string building; for large programs
  consider a string-builder approach.

## Roadmap

- [x] SC0 -- lexer
- [x] SC1 -- parser
- [x] SC2 -- codegen
- [x] SC3 -- expander (special forms + quasiquote + user defmacro)
- [x] SC4 -- stdlib macros + compile API
- [x] SC5 -- tests + README
- [ ] SC6 -- `scscm/server`: OSC client for scsynth/hcsynth (requires `tur-osc`)
