# turmeric-spices -- Claude Code Guide

This repo holds the spice packages that build against the `tur` compiler from
the sibling repo `rjungemann/turmeric`. The compiler is **not** built from
source here -- fetch a prebuilt binary instead.

## Getting the `tur` binary (do this first on any new sandbox)

Run the install script. It downloads the matching prebuilt release tarball
from `rjungemann/turmeric`, verifies the SHA-256, extracts it under
`vendor/tur/`, and prints the export line you need:

```sh
./scripts/install-tur.sh
eval "$(./scripts/install-tur.sh)"   # or: export TUR_BIN="$PWD/vendor/tur/tur"
```

- Pin a specific version: `TUR_VERSION=v0.13.0 ./scripts/install-tur.sh`
- Force a redownload: `./scripts/install-tur.sh --force`
- Supported platforms: `macos-arm64`, `linux-x86_64`, `linux-aarch64`
- The release pipeline lives at `rjungemann/turmeric/.github/workflows/release.yml`
  and runs on every `v*` tag push. If `releases/latest` returns nothing, the
  most recent tag predates the pipeline -- pin `TUR_VERSION` to a tag that has
  binary assets attached, or push a new tag from the turmeric repo.

**Do not build `tur` from source as a default.** Source builds are the
fallback when no release asset exists for your platform, not the happy path.
Each cold sandbox doing a CMake build wastes minutes per session.

## Repo layout

```
spices/        -- one directory per spice package; each has its own build.tur
docs/          -- shared documentation
build.tur      -- top-level manifest
vendor/tur/    -- prebuilt tur binary + stdlib (gitignored; created by install-tur.sh)
scripts/       -- developer tooling (install-tur.sh, ...)
```

The tarball that `install-tur.sh` extracts contains `tur`, `libturi.a`,
`include/turi/*.h`, and the full `stdlib/` tree. The stdlib lives next to
the binary on purpose -- do not move them apart.

## CI

`.github/workflows/ci.yml` currently checks out `rjungemann/turmeric` and
builds `tur` from source on every run. That is intentional for CI's
reproducibility guarantees (CI verifies spices against tip-of-main turmeric,
not against a release). For **local** and **agent sandbox** work, prefer the
prebuilt path above.
