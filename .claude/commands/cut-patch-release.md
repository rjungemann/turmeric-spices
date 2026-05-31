---
description: Cut a new patch release of a single spice. Bump build.tur :version, update CHANGELOG, deploy docs, push tag.
argument-hint: <spice-name>
allowed-tools: Bash, Read, Edit, AskUserQuestion
---

# Cut a new patch release of a spice

Run a full patch-version release of one spice in this workspace.
Versioning is **per-spice**: each `spices/<name>/build.tur` carries its
own `:version`, and each spice is tagged independently as
`<name>-v<MAJOR>.<MINOR>.<PATCH>`.

Order matters: the commit must contain the bumped version (and the
CHANGELOG update if one exists) before the tag is created, the docs
deploy must succeed before the tag is pushed (so a deploy failure
doesn't strand a tag), and the tag push must include the bump commit so
any downstream consumer that pins the tag sees the right sources.

## Argument

`$1` -- the spice name (e.g. `ansi`, `notebook`, `signal`).

If `$1` is empty, refuse and ask the user which spice to release. If
`spices/<name>/build.tur` does not exist, refuse and list the available
spices (`ls spices/`).

## Preconditions (verify before doing anything destructive)

Run these in parallel and report findings before proceeding:

1. `git status --porcelain` -- working tree must be clean. If not,
   stop and ask the user to commit or stash.
2. `git rev-parse --abbrev-ref HEAD` -- must be `main`. If not, stop
   and ask the user to switch.
3. `git fetch origin main` followed by `git rev-list --left-right --count origin/main...HEAD`
   -- local main must not be behind origin. If behind, stop and ask the
   user to pull.
4. `grep ':version' spices/<name>/build.tur` -- current version (the
   old version). Extract the `"X.Y.Z"` string.
5. `git describe --tags --abbrev=0 --match '<name>-v*'` -- the most
   recent release tag for this spice. Should match `<name>-v<VERSION>`;
   if not, surface the mismatch to the user before proceeding. If no
   tag exists at all for this spice, surface that too and ask whether
   to proceed (this would be the first tagged release).
6. `git log <name>-v<OLD>..HEAD --oneline -- spices/<name>` -- there
   must be at least one commit touching this spice since the last tag.
   If zero, refuse to release. (If there is no prior tag, skip the
   `<tag>..HEAD` range and just confirm the spice has commits at all.)

If any check fails, stop and report. Do not proceed without the user
explicitly overriding.

## Step 1: Compute the new version

Read `:version` from `spices/<name>/build.tur`. Parse `MAJOR.MINOR.PATCH`.
Compute `NEW = MAJOR.MINOR.(PATCH+1)`.

Example: `0.1.4` -> `0.1.5`.

## Step 2: Draft the CHANGELOG entry

Run `git log <name>-v<OLD>..HEAD --pretty=format:'%h %s' -- spices/<name>`
to get the commit list since the last tag, scoped to this spice's
directory. (If there is no prior tag, run `git log --pretty=format:'%h %s' -- spices/<name>`
and pick a reasonable cutoff with the user.)

Classify each commit into one of:
- **Added** -- new features, new exports, new modules within this spice
- **Changed** -- behavior changes, renames, semantic shifts in existing features
- **Fixed** -- bug fixes (commits starting with `fix:`, "fix", or referencing a bug)
- **Removed** -- deletions of features, modules, or exports
- **Docs** -- documentation-only changes (only include if non-trivial)
- **Internal** -- skip from changelog (CI, refactors with no user-visible effect, dependency bumps)

Skim each commit's subject line and, when ambiguous, run
`git show --stat <sha>` to see what files changed. Don't include every
internal commit -- the changelog audience is users of the spice.

Format the new entry to match the existing per-spice CHANGELOG style
(see `spices/plot/CHANGELOG.md` or `spices/frame/CHANGELOG.md` for
reference -- they use `## <VERSION>` headers, not `## [<VERSION>]`):

```
## NEW

### Added
- ...

### Changed
- ...

### Fixed
- ...
```

Omit empty subsections. Aim for 2-8 bullets total -- consolidate
related commits into one bullet rather than copying every subject line.

If `spices/<name>/CHANGELOG.md` does **not** exist, create it with this
template:

```
# Changelog

## NEW

### ...
- ...
```

## Step 3: Confirm with the user

Show the user:
- The spice name and the OLD -> NEW version transition
- The full CHANGELOG entry you drafted
- Whether `spices/<name>/CHANGELOG.md` already exists or will be created
- The list of commits that informed the changelog

Use `AskUserQuestion` with options:
- **Proceed**: continue with steps 4-8 as drafted
- **Edit the changelog**: ask the user what to change, then re-show
- **Cancel**: stop without making any changes

Do not proceed past this step without explicit confirmation.

## Step 4: Apply file changes (no git operations yet)

In parallel:
1. Edit `spices/<name>/build.tur` -- replace the `:version "<OLD>"`
   line with `:version "<NEW>"`.
2. Edit (or create) `spices/<name>/CHANGELOG.md` -- insert the new
   entry immediately after the `# Changelog` header and before the
   most recent existing `## <OLD>` entry. Keep one blank line between
   entries.

After applying, run `git diff --stat` and show the user what changed.
Do not commit yet.

## Step 5: Commit locally

```sh
git add spices/<name>/build.tur spices/<name>/CHANGELOG.md
git commit -m "$(cat <<'EOF'
chore(<name>): release <name>-v<NEW>

<one-paragraph summary copied from the changelog's most significant items>

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Then create the annotated tag pointing at this new commit:

```sh
git tag -a "<name>-v<NEW>" -m "Release <name>-v<NEW>"
```

The tag exists locally only; nothing is pushed yet. If the docs deploy
in step 6 fails, you can delete the local tag and try again without
having published a broken release.

## Step 6: Build and deploy docs

```sh
just deploy-web
```

This regenerates the per-spice docs site (`just docs` -> `just guides`)
and pushes it to Cloudflare via `wrangler`. The user must already be
authenticated with `wrangler`.

If `just deploy-web` fails:
- Report the failure to the user.
- Run `git tag -d <name>-v<NEW>` to remove the local tag.
- Do NOT delete the commit -- the user can amend or fix forward.
- Stop. Do not proceed to step 7.

If deploy is intentionally skipped (no docs changes; user opts out),
ask the user to confirm explicitly before proceeding to step 7.

## Step 7: Push commit and tag

Only after a successful deploy (or explicit skip):

```sh
git push origin main
git push origin "<name>-v<NEW>"
```

The tag is the canonical reference downstream consumers of this spice
will pin against.

## Step 8: Verify

Report to the user:
- The spice name and new version
- The commit SHA of the bump commit
- The full tag name (`<name>-v<NEW>`)
- The Cloudflare deploy URL or "deployed" confirmation
- The most recent few entries from `git tag --list '<name>-v*' | tail`
  so the user can see this tag landed alongside its predecessors.

## Things to refuse

- Refuse to bypass any precondition without explicit user override.
- Refuse to release a spice whose `build.tur` is missing or malformed.
- Refuse to push the tag before the deploy succeeds (unless the user
  explicitly opted out of deploy).
- Refuse to skip the CHANGELOG update -- per-spice CHANGELOGs are the
  primary release-notes surface for spice consumers.
- Refuse to use `git push --force` for any step here.
- Refuse to amend a commit that has already been pushed.
- Refuse to touch any spice other than `<name>` -- if commits since
  the last tag spanned multiple spices, surface that and ask the user
  whether to proceed (the tag will still cover only this spice's bump).
