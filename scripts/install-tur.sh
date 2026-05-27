#!/usr/bin/env bash
# Fetch a prebuilt `tur` binary from the rjungemann/turmeric GitHub Release
# matching this host's platform, extract it to vendor/tur/, and print the
# export commands the caller needs.
#
# Usage:
#   ./scripts/install-tur.sh                 # latest published release
#   TUR_VERSION=v0.13.0 ./scripts/install-tur.sh
#   ./scripts/install-tur.sh --force         # redownload even if cached
#
# Notes:
#   - Public release, no auth needed. Uses curl + tar + shasum, plus jq if
#     present (falls back to a grep parser otherwise).
#   - Supported targets: linux-x86_64, linux-aarch64, macos-arm64.
#   - Tarball layout (from turmeric's release.yml): ./tur, ./libturi.a,
#     ./include/turi/*.h, ./stdlib/. The stdlib must stay next to `tur`.

set -euo pipefail

REPO="rjungemann/turmeric"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/vendor/tur"
FORCE=0
for arg in "$@"; do
  case "$arg" in
    --force) FORCE=1 ;;
    -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown arg: $arg" >&2; exit 2 ;;
  esac
done

# Detect platform triple matching turmeric's release.yml matrix.
uname_s="$(uname -s)"
uname_m="$(uname -m)"
case "$uname_s/$uname_m" in
  Darwin/arm64)   TARGET="macos-arm64" ;;
  Linux/x86_64)   TARGET="linux-x86_64" ;;
  Linux/aarch64)  TARGET="linux-aarch64" ;;
  *) echo "unsupported platform: $uname_s/$uname_m" >&2
     echo "turmeric publishes: macos-arm64, linux-x86_64, linux-aarch64" >&2
     exit 1 ;;
esac

# Resolve version.
if [ -n "${TUR_VERSION:-}" ]; then
  TAG="$TUR_VERSION"
else
  api="https://api.github.com/repos/$REPO/releases/latest"
  body="$(curl -sSL -w '\n%{http_code}' "$api")"
  status="${body##*$'\n'}"
  body="${body%$'\n'*}"
  if [ "$status" != "200" ]; then
    echo "no published (non-prerelease) release found for $REPO (HTTP $status)" >&2
    echo "set TUR_VERSION=vX.Y.Z explicitly, or browse https://github.com/$REPO/releases" >&2
    exit 1
  fi
  if command -v jq >/dev/null 2>&1; then
    TAG="$(printf '%s' "$body" | jq -r .tag_name)"
  else
    TAG="$(printf '%s' "$body" | grep -m1 '"tag_name"' | sed -E 's/.*"tag_name": *"([^"]+)".*/\1/')"
  fi
  if [ -z "$TAG" ] || [ "$TAG" = "null" ]; then
    echo "could not parse tag_name from $api response" >&2
    exit 1
  fi
fi

stamp="$DEST/.version"
if [ "$FORCE" -eq 0 ] && [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$TAG/$TARGET" ]; then
  echo "already installed: $TAG ($TARGET) at $DEST" >&2
  echo "export TUR_BIN=\"$DEST/tur\""
  exit 0
fi

archive="turmeric-${TAG}-${TARGET}.tar.gz"
base="https://github.com/$REPO/releases/download/$TAG"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "downloading $archive from $TAG..." >&2
curl -fsSL "$base/$archive"        -o "$tmp/$archive"
curl -fsSL "$base/sha256sums.txt"  -o "$tmp/sha256sums.txt"

# Verify checksum (sha256sums.txt has one line per platform; grep ours).
expected="$(grep -E "  $archive$" "$tmp/sha256sums.txt" | awk '{print $1}')"
if [ -z "$expected" ]; then
  echo "no sha256 entry for $archive in sha256sums.txt" >&2
  exit 1
fi
actual="$(shasum -a 256 "$tmp/$archive" | awk '{print $1}')"
if [ "$expected" != "$actual" ]; then
  echo "sha256 mismatch for $archive" >&2
  echo "  expected: $expected" >&2
  echo "  actual:   $actual"   >&2
  exit 1
fi

rm -rf "$DEST"
mkdir -p "$DEST"
tar -xzf "$tmp/$archive" -C "$DEST"
chmod +x "$DEST/tur"
echo "$TAG/$TARGET" > "$stamp"

echo "installed $TAG ($TARGET) to $DEST" >&2
"$DEST/tur" --version >&2 || true
echo
echo "export TUR_BIN=\"$DEST/tur\""
