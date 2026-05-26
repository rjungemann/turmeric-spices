# tur-notebook

`tur-notebook` adds literate `.tur.md` notebooks to the Turmeric ecosystem.

The current implementation covers **NB0-NB12** from
`docs/notebook-spice-plan.md`: the spice manifest, markdown parsing, notebook
cell extraction, session-backed evaluation, caching, markdown rendering,
standalone HTML export, the terminal TUI (`tur nb tui`) with editor-backed
cell editing, insert/delete/paste actions, dirty-save quit handling, search
(`/`, `n`, `N`), a help overlay (`?`), focused-output toggling (`o`), and
user keybinding override files (`--keybindings`, merged onto the built-in map),
plus an image-hook display system (NB11) and `exec`/`new` CLI subcommands (NB12).

`src/notebook/style.css` is the vendored notebook HTML stylesheet snapshot. If
the docs-site styling changes meaningfully, refresh this file and keep the
embedded CSS in `src/notebook/render-html.tur` in sync.

## CLI Subcommands

```sh
# Render a notebook to terminal markdown
nb render notebook.tur.md

# Export to standalone HTML
nb export notebook.tur.md -o out.html

# Launch the interactive TUI
nb tui notebook.tur.md

# Run all cells and print their outputs
nb exec --all notebook.tur.md

# Run cells starting from a specific cell id
nb exec --cell my-cell notebook.tur.md

# Create a new starter notebook
nb new notebook.tur.md
```

## Image Hook (NB11)

Cells that produce images can advertise them to the TUI via:

```turmeric
(import notebook/image :refer [image-hook-record-path])
(image-hook-record-path "/path/to/output.png")
```

The TUI renders PNG images inline using the Kitty graphics protocol, iTerm2
inline images, or a text fallback for other terminals.
