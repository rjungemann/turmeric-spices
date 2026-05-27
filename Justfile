# Generate per-spice HTML docs from this repo's spices/ directory.
docs:
    python3 tools/genspices.py --out docs/html/ --emit-json docs/html/doc-names-spices.json

# Deploy docs to spices.turmeric-lang.com via Cloudflare.
# Requires wrangler to be authenticated (wrangler login).
deploy-web: docs
    rm -rf web/dist
    mkdir -p web/dist
    cp -R docs/html/. web/dist/
    cp -R web/public/. web/dist/
    cp -R web/styles web/dist/styles
    cd web && npm install && npm run deploy

# Validate turmeric+sweet-exp code pairs in all spice READMEs.
check-docs:
    python3 tools/check-guide-pairs.py --spices
