# `web/styles/` -- the spices site's stylesheet

`style.css` is the **one** stylesheet for spices.turmeric-lang.com. Two
consumers read it, and both read this same file:

- `just deploy-web` copies it to `web/dist/styles/style.css`, served at
  `/styles/style.css` -- what the landing page, the guides and the per-spice
  front pages link.
- `tools/gendocs.py` reads it at import time and writes it next to every
  generated API tree as `<spice>/api/style.css`.

So a spice's API reference and the rest of the site cannot say different
things. They used to: `gendocs.py` carried its own inline copy of the CSS, and
the fix that made sidebars sans-serif (#73) landed in this file only -- which
is why every `<spice>/api/` page kept rendering its sidebar in monospace.

## It is a copy of the main site's stylesheet -- keep it byte-identical

This file is the main site's generated API stylesheet, verbatim. That is the
point: spices.turmeric-lang.com is a subdomain of turmeric-lang.com, not a
separate product, and the fastest way for it to start looking like a different
site is for this copy to drift.

To re-sync it against the main site, take the stylesheet the turmeric repo's
`tools/gendocs.py` emits:

```sh
# from a turmeric checkout, after `just docs`
cp ../turmeric/docs/html/api/style.css web/styles/style.css

# or straight from the deployed site
curl -o web/styles/style.css https://turmeric-lang.com/docs/html/api/style.css
```

and check that nothing changed that this site needs and does not have:

```sh
diff <(curl -s https://turmeric-lang.com/docs/html/api/style.css) web/styles/style.css
```

An empty diff is the goal. If a rule has to differ for this site, it belongs in
a page's own `<style>` block (the generators each have one), not in a local
edit here -- a local edit is invisible until the next sync silently reverts it.

The stylesheet is keyed on the class names `tools/sitechrome.py` emits
(`nav.nav-links`, `.nav-right`, `.btn-ghost`/`.btn-gold`, `a.sidebar-back`,
`.sidebar-uplinks`, `a.sidebar-def`). Changing the chrome markup without
changing both ends is what drops the styling, silently and only on this site.
