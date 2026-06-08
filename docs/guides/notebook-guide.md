# tur-notebook -- Notebook Guide

> Spice version 0.1.0 -- Literate `.tur.md` notebooks with a static renderer
> and an interactive terminal TUI.
> Audience: Turmeric users who want a Jupyter-style workflow for exploratory
> code, reproducible analyses, and shareable HTML reports.
>
> A `.tur.md` file is a strict superset of CommonMark: ordinary markdown that
> renders cleanly in GitHub / VS Code / Obsidian / pandoc, where fenced code
> blocks tagged `turmeric` or `sweet-exp` are executable cells. Pair with
> [`tur-frame`](frame-guide.md) for data loading, [`tur-plot`](https://github.com/rjungemann/turmeric-spices/tree/main/spices/plot)
> for figures, or [`tur-stats`](https://github.com/rjungemann/turmeric-spices/tree/main/spices/stats) for analysis.

This guide walks through the workflow:

1. [Writing your first `.tur.md`](#1-your-first-notebook)
2. [Rendering: markdown vs HTML, watch mode](#2-rendering)
3. [The TUI: command mode and editing](#3-the-tui)
4. [Caching expensive cells](#4-caching)
5. [Embedding plots and images](#5-plots-and-images)
6. [Reproducibility and CI](#6-reproducibility-and-ci)
7. [Customizing keybindings](#7-customizing-keybindings)

---

## 0. Installing the spice

In your project's `build.tur`:

```turmeric
:spices #{
  "notebook" #{:url    "https://github.com/rjungemann/turmeric-spices"
               :ref    "notebook-v0.1.0"
               :subdir "spices/notebook"}
}
```

Then:

```sh
tur fetch
tur install tur-notebook    ;; puts `tur-nb` on $PATH
```

No external C dependencies -- the parser, renderer, and TUI are pure Turmeric
plus a vendored libturi for in-process cell execution.

---

## 1. Your first notebook

Scaffold a starter file:

```sh
tur nb new analysis.tur.md
```

Open it in any editor. The body looks like ordinary markdown -- prose, headings,
lists -- except that fenced `turmeric` blocks are *cells*:

````markdown
# My Analysis

Some prose explaining what we're about to do.

```turmeric
(+ 1 2)
```

More prose.

```turmeric {id=greeting}
(println "hello, notebook")
```
````

Cells have optional attributes inside `{...}` on the fence line (Quarto
style). The most common:

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `id`      | auto (`cell-1`, `cell-2`, ...) | Stable handle for `--cell` and TUI navigation |
| `eval`    | `true`  | Set `false` to render without executing |
| `echo`    | `true`  | Set `false` to hide the source in rendered output |
| `output`  | `true`  | Set `false` to suppress the output block |
| `error`   | `halt`  | `continue` records the error and proceeds |
| `cache`   | `false` | Cache by source hash under `.turnb-cache/` |
| `depends` | (none)  | Comma-separated cell ids this cell depends on |
| `image`   | `inline`| `inline` = base64 in rendered file; `file` = sibling PNG |

Cells tagged `sweet-exp` use [sweet-expression syntax](#) -- everything else
about the workflow is identical.

---

## 2. Rendering

Render to markdown (the default):

```sh
tur nb render analysis.tur.md            # writes analysis.md
```

Render to a standalone HTML page:

```sh
tur nb render analysis.tur.md --to html  # writes analysis.html
```

`tur nb export` is a more discoverable alias for the same workflow:

```sh
tur nb export md   analysis.tur.md
tur nb export html analysis.tur.md
tur nb export html analysis.tur.md --out site/    # write into a directory
tur nb export md   analysis.tur.md --no-output    # strip output blocks
tur nb export md   analysis.tur.md --no-source    # outputs only
```

For "I'm writing prose, just keep the rendered file fresh," watch mode
re-renders on every save:

```sh
tur nb render analysis.tur.md --watch
```

Watch mode uses `kqueue` on macOS and `inotify` on Linux. Each re-render
starts with a fresh session -- predictability over warm caches. If you want
warm-cache exploration, use the TUI instead.

---

## 3. The TUI

```sh
tur nb tui analysis.tur.md
```

The TUI is **modal**, in the Jupyter / vim style. In *command mode*, single
keys navigate and re-run cells:

| Key | Action |
|-----|--------|
| `j` / `k`        | Move focus down / up |
| `gg` / `G`       | Jump to first / last cell |
| `Enter`          | Re-run the focused cell |
| `Shift-Enter`    | Run focused cell, then move to the next |
| `R`              | Restart session and re-run all |
| `r`              | Re-run from the focused cell onward |
| `e`              | Edit the focused cell (`$EDITOR`) |
| `a` / `b`        | Insert a new cell above / below |
| `dd` / `p`       | Delete (yank) / paste a cell |
| `o`              | Toggle output visibility |
| `s`              | Save the file |
| `/`, `n`, `N`    | Search across cell sources and outputs |
| `?`              | Help overlay |
| `q`              | Quit (prompts if dirty) |

Hitting `e` writes the focused cell to a temp file and spawns `$EDITOR` on
it; on exit, the cell source is replaced and the file marked dirty. The TUI
does **not** ship its own text editor -- you get the keybindings, theme, and
plugins you have already configured for vim / helix / nano / emacs / VS Code.

The interpreter session lives for the lifetime of the TUI process: definitions
made in one cell are visible in later ones, exactly like a Jupyter kernel.
`R` is the "clean slate" key when you want to verify a notebook runs from a
fresh session.

---

## 4. Caching

For cells that are slow to recompute (loading a large CSV, fitting a model),
opt in to source-hash caching:

````markdown
```turmeric {id=load-data cache=true}
(import frame/csv :refer [read-csv default-csv-opts])
(def iris (read-csv "iris.csv" (default-csv-opts)))
```

```turmeric {id=fit-model cache=true depends=load-data}
(def model (fit iris))
```
````

The cache key is `SHA-256(cell-source + sorted-attrs + dependency-hashes)`.
Editing `load-data` busts every downstream cell that lists it in `depends`.
Pass `--cache` to enable the cache for `render` / `export`:

```sh
tur nb render analysis.tur.md --cache
```

The cache lives in `.turnb-cache/` beside the source file -- add it to
`.gitignore`.

---

## 5. Plots and images

`tur-notebook` does not require any plotting library. To embed an image, a
cell writes a PNG and announces its path via the image hook:

```turmeric
(import notebook/image :refer [image-hook-record-path])
(import plot/core      :refer [plot-write-png])

(plot-write-png renderers opts "iris-scatter.png")
(image-hook-record-path "iris-scatter.png")
```

The hook works via a stdout marker (`__NB_IMG__: <path>`) that the session
intercepts before display. Cells get the path back as part of their
`cell-output.image-paths` list, and the TUI / renderers do the right thing
with it:

- **HTML render**: the PNG is embedded inline as a base64 data URL (or as
  a sibling-file `<img src=...>` link when `image=file` is set on the cell).
- **Markdown render**: the PNG is emitted as a `![](data:image/png;base64,...)`
  tag, so the rendered `.md` is self-contained.
- **TUI**: detects terminal support and uses the
  [Kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/)
  or the [iTerm2 inline image protocol](https://iterm2.com/documentation-images.html)
  to draw the image directly in the output region. Terminals without
  image support get a `[image: path]` placeholder; the file is still on
  disk and openable from the shell.

This is the same opt-in pattern `tur-plot` and `tur-plutovg` use: cells that
write PNGs can advertise them, but the spices themselves stay independent of
the notebook tooling.

---

## 6. Reproducibility and CI

Notebooks that use randomness (any cell calling into `tur-stats`'s `rng-*`
or any PRNG) should pass an **explicit seed**. The notebook tooling does
*not* auto-seed -- doing so would make notebooks that look reproducible
silently non-reproducible the moment they are edited. Spell the seed in
user code:

```turmeric
(import stats/rng :refer [rng-make])
(def rng (rng-make 42))
```

For CI, the `exec` subcommand runs cells without writing output blocks back
to disk:

```sh
tur nb exec analysis.tur.md --all              # run every cell, print outputs
tur nb exec analysis.tur.md --cell fit-model   # run from this cell onward
```

A non-zero exit code means at least one cell errored, so this composes
cleanly with `set -e` in a CI script:

```sh
#!/bin/sh
set -e
for nb in notebooks/*.tur.md; do
  tur nb exec "$nb" --all >/dev/null
done
```

Combine with deterministic seeds and you can diff notebook outputs in version
control to catch regressions in numerical behavior.

---

## 7. Customizing keybindings

The TUI's defaults live in `notebook/keys.tur`. Override them with a
file passed to `--keybindings`:

```
# ~/.turnb-keys
# one "key action" per line; # starts a comment
j        cell-next
k        cell-prev
<Enter>  run-cell
e        edit-cell
R        restart-and-run-all
q        quit
```

```sh
tur nb tui analysis.tur.md --keybindings ~/.turnb-keys
```

User bindings are merged onto the built-in defaults; only the actions you
list are overridden, so a minimal file is fine. The available actions are
documented in `notebook/keys.tur` (`default-keybindings`).

`--no-color` disables ANSI colors entirely -- useful for terminals that do
not handle 256-color escapes well, or for screen-readers.

---

## Limitations in v0.1.0

- **No undo for cell-level edits.** Insert / delete / paste are not yet
  reversible from inside the TUI. The save-on-quit prompt protects against
  accidental loss; full undo is planned for v0.2.
- **HTML blocks and reference-style links** are not parsed by the included
  CommonMark subset; they render as their literal source text. Inline links,
  GFM tables, task lists, and strikethrough are supported.
- **Terminal image protocols vary.** Inline images work in Kitty, iTerm2,
  and WezTerm; other terminals fall back to a `[image: path]` placeholder
  in the TUI. Rendered HTML and markdown carry the image regardless.

The parser scope and the included / deferred features are documented in
`docs/notebook-spice-plan.md` -- file an issue if a missing CommonMark feature
is blocking real notebook work.
