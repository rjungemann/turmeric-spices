# Generate per-spice HTML docs from this repo's spices/ directory.
docs:
    python3 tools/genspices.py --out docs/html/ --emit-json docs/html/doc-names-spices.json

# Run the spices doc site locally via wrangler dev.
web-dev: docs
    rm -rf web/dist
    mkdir -p web/dist
    cp -R docs/html/. web/dist/
    cp -R web/public/. web/dist/
    cp -R web/styles web/dist/styles
    cd web && npm install && npx wrangler dev

# Deploy docs to spices.turmeric-lang.com via Cloudflare.
# Requires wrangler to be authenticated (wrangler login).
deploy-web: docs
    rm -rf web/dist
    mkdir -p web/dist
    cp -R docs/html/. web/dist/
    cp -R web/public/. web/dist/
    cp -R web/styles web/dist/styles
    cd web && npm install && npm run deploy

# Download and vendor a specific KaTeX release into the notebook spice.
# Usage: just vendor-katex 0.16.21
vendor-katex version:
    #!/usr/bin/env bash
    set -euo pipefail
    DEST="spices/notebook/src/notebook/vendor/katex"
    TMP=$(mktemp -d)
    trap 'rm -rf "$TMP"' EXIT
    curl -fsSL "https://registry.npmjs.org/katex/-/katex-{{version}}.tgz" -o "$TMP/katex.tgz"
    tar -xzf "$TMP/katex.tgz" -C "$TMP"
    mkdir -p "$DEST/fonts"
    cp "$TMP/package/dist/katex.min.js" "$DEST/katex.min.js"
    cp "$TMP/package/dist/katex.min.css" "$DEST/katex.min.css"
    cp "$TMP/package/dist/contrib/auto-render.min.js" "$DEST/auto-render.min.js"
    cp "$TMP/package/LICENSE" "$DEST/LICENSE"
    echo "{{version}}" > "$DEST/VERSION"
    cp "$TMP"/package/dist/fonts/*.woff2 "$DEST/fonts/"
    echo "Vendored KaTeX {{version}} into $DEST"

# Validate turmeric+sweet-exp code pairs in all spice READMEs.
check-docs:
    python3 tools/check-guide-pairs.py --spices
