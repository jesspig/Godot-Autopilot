#!/usr/bin/env bash
set -euo pipefail

DIR="dist"
TAG=""
REPO="${GDA_ATOMGIT_REPO:-}"
API="${GDA_ATOMGIT_API:-}"
TOKEN="${GDA_ATOMGIT_TOKEN:-}"
DRY_RUN=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --dir) DIR="$2"; shift 2 ;;
    --tag) TAG="$2"; shift 2 ;;
    --repo) REPO="$2"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    --help|-h)
      printf 'usage: mirror_atomgit.sh --dir dist --tag VERSION --repo owner/repo [--dry-run]\n'
      exit 0
      ;;
    *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
  esac
done

if [ ! -d "$DIR" ]; then
  printf 'asset directory not found: %s\n' "$DIR" >&2
  exit 2
fi
if [ -z "$TAG" ] || [ -z "$REPO" ]; then
  printf '--tag and --repo (or GDA_ATOMGIT_REPO) are required\n' >&2
  exit 2
fi

ASSETS=(
  addons.zip
  "gda-${TAG}-windows-x64.zip"
  "gda-${TAG}-linux-x64.tar.gz"
  "gda-${TAG}-linux-x64-musl.tar.gz"
  "gda-${TAG}-macos-arm64.tar.gz"
  "gda-${TAG}-macos-x64.tar.gz"
  skills-en.zip
  skills-zh.zip
)

missing=0
for asset in "${ASSETS[@]}"; do
  if [ ! -f "$DIR/$asset" ]; then
    printf 'missing asset: %s\n' "$DIR/$asset" >&2
    missing=1
  fi
done
if [ "$missing" -ne 0 ]; then
  exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
  (cd "$DIR" && sha256sum "${ASSETS[@]}" > SHA256SUMS)
else
  (cd "$DIR" && shasum -a 256 "${ASSETS[@]}" > SHA256SUMS)
fi

if [ "$DRY_RUN" -eq 1 ]; then
  printf 'atomgit repo: %s\n' "$REPO"
  printf 'tag: %s\n' "$TAG"
  printf 'api: %s\n' "${API:-<default>}"
  cat "$DIR/SHA256SUMS"
  exit 0
fi

if [ -z "$API" ] || [ -z "$TOKEN" ]; then
  printf 'GDA_ATOMGIT_API and GDA_ATOMGIT_TOKEN are required for upload\n' >&2
  exit 2
fi

for asset in "${ASSETS[@]}" SHA256SUMS; do
  printf 'uploading %s\n' "$asset"
  curl -fsSL -X POST \
    -H "Authorization: Bearer ${TOKEN}" \
    -F "file=@${DIR}/${asset}" \
    "${API%/}/${REPO}/releases/${TAG}/assets?name=${asset}" >/dev/null
done

printf 'mirrored %s assets to AtomGit %s (%s)\n' "${#ASSETS[@]}" "$REPO" "$TAG"
