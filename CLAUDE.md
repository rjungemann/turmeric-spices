# turmeric-spices -- Claude Code Guide

This repo holds the spice packages that build against the `tur` compiler from
the sibling repo `rjungemann/turmeric`. The compiler is **not** built from
source here -- fetch a prebuilt binary instead.

## Reading the sibling `turmeric` repo -- STRICT RULE

Reading source, docs, fixtures, plans, or history from `rjungemann/turmeric`
is **always allowed**, even when it is not checked out locally. The
single-repo sandbox is a working directory, not an enforcement boundary.
"We only have spices checked out" is **never** a valid reason to refuse to
look something up in turmeric -- a stdlib signature, a plan under
`docs/upcoming/`, an archived report under `docs/archive/`, a fixture, a
commit message, or anything else the user references by path or name.

Use whichever fetch path is convenient:

- `gh api repos/rjungemann/turmeric/contents/<path>` (single file, base64).
- `gh api repos/rjungemann/turmeric/git/trees/main?recursive=1` (full tree listing).
- `git clone --depth=1 https://github.com/rjungemann/turmeric /tmp/turmeric`
  then grep/read locally. `/tmp/turmeric` is a read-only scratch copy; do not
  treat it as a second working tree, do not commit to it, do not push from it.
- `WebFetch` on a `https://raw.githubusercontent.com/rjungemann/turmeric/main/<path>` URL.

If a user mentions a turmeric concept (e.g. "yyjson plan", "tur-signal gate",
a doc filename) and the file is not in this repo, **fetch it before claiming
ignorance or blockage**. Quoting a passage from the turmeric repo back to the
user is a normal read operation, not a cross-repo violation.

What you **cannot** do from a turmeric-spices-rooted session: open PRs,
push branches, or land commits against `rjungemann/turmeric`. That constrains
writes only; it does not constrain reads. If the work the user wants requires
*writing* to turmeric, stop and tell them to re-launch a session rooted there
(or do it from local Claude Code); do not silently degrade to "I can't help."

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

## Cross-spice development (workspace-local imports)

All spices in this repo are listed as `:members` of the root `build.tur`.
That makes them a workspace, so one sibling can import another **without
`tur fetch`, a symlink, or a lockfile entry**.

```sh
cd spices/notebook
tur check src/notebook/cli.tur   # resolves watch/watch via workspace member
```

The first undeclared sibling import prints a one-time advisory:

```
warning: import 'watch/watch' resolved via workspace sibling 'spices/watch';
         declare it in :spices for release builds.
```

To declare a local dep explicitly (for editor/LSP autocomplete or to avoid the
advisory), use a `:path` entry in `build.tur`:

```turmeric
:spices {
  "watch" {:path "../watch"}
}
```

`tur fetch --dry-run` shows which deps would be fetched vs skipped locally.
Local-source deps never produce a `tur.lock` entry. Do not hand-edit
`tur.lock` to add stub entries for workspace siblings -- that workaround is
no longer needed.

## CI

`.github/workflows/ci.yml` currently checks out `rjungemann/turmeric` and
builds `tur` from source on every run. That is intentional for CI's
reproducibility guarantees (CI verifies spices against tip-of-main turmeric,
not against a release). For **local** and **agent sandbox** work, prefer the
prebuilt path above.
