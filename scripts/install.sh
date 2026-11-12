#!/usr/bin/env bash
set -euo pipefail

VERSION=""
PREFIX="${HOME}/.godot-autopilot/bin"
ARCHIVE=""
NO_PATH=0
DRY_RUN=0
GITHUB_BASE="${GDA_GITHUB_BASE:-https://github.com/jesspig/Godot-Autopilot/releases}"
ATOMGIT_BASE="${GDA_ATOMGIT_BASE:-}"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --version) VERSION="$2"; shift 2 ;;
    --prefix) PREFIX="$2"; shift 2 ;;
    --archive) ARCHIVE="$2"; shift 2 ;;
    --no-path) NO_PATH=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    --help|-h) printf 'usage: install.sh [--version V] [--prefix DIR] [--archive PATH] [--no-path] [--dry-run]\n'; exit 0 ;;
    *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
  esac
done

run() {
  if [ "$DRY_RUN" -eq 1 ]; then
    printf '[dry-run] %s\n' "$*"
  else
    "$@"
  fi
}

detect_platform() {
  os="$(uname -s)"
  arch="$(uname -m)"
  case "$os" in
    Darwin)
      case "$arch" in
        arm64) echo "macos-arm64 tar.gz" ;;
        x86_64) echo "macos-x64 tar.gz" ;;
        *) printf 'unsupported macOS arch: %s\n' "$arch" >&2; exit 2 ;;
      esac
      ;;
    Linux)
      if ldd --version 2>&1 | grep -qi musl; then
        echo "linux-x64-musl tar.gz"
      else
        echo "linux-x64 tar.gz"
      fi
      ;;
    MINGW*|MSYS*|CYGWIN*)
      echo "windows-x64 zip"
      ;;
    *)
      printf 'unsupported OS: %s\n' "$os" >&2
      exit 2
      ;;
  esac
}

read -r PLATFORM EXT <<<"$(detect_platform)"
if [ -z "$VERSION" ]; then
  VERSION="${GDA_VERSION:-}"
fi
if [ -z "$VERSION" ]; then
  if [ -n "$ARCHIVE" ] && [ -d "$ARCHIVE" ] && [ -f "$ARCHIVE/VERSION" ]; then
    VERSION="$(tr -d ' \t\r\n' < "$ARCHIVE/VERSION")"
  else
    printf 'a version is required (--version or GDA_VERSION)\n' >&2
    exit 2
  fi
fi
ASSET="gda-${VERSION}-${PLATFORM}.${EXT}"
CACHE_DIR="${HOME}/.godot-autopilot/cache/cli/${VERSION}/${PLATFORM}"

verify_checksum() {
  local file="$1" sums="$2"
  if [ ! -f "$sums" ]; then
    printf 'warning: SHA256SUMS missing; skipping checksum\n' >&2
    return 0
  fi
  local expected
  expected="$(grep "  ${ASSET}$" "$sums" | awk '{print $1}' || true)"
  if [ -z "$expected" ]; then
    printf 'error: %s is not listed in SHA256SUMS\n' "$ASSET" >&2
    exit 1
  fi
  local actual
  if command -v sha256sum >/dev/null 2>&1; then
    actual="$(sha256sum "$file" | awk '{print $1}')"
  else
    actual="$(shasum -a 256 "$file" | awk '{print $1}')"
  fi
  if [ "$expected" != "$actual" ]; then
    printf 'error: checksum mismatch for %s\n' "$ASSET" >&2
    exit 1
  fi
}

fetch() {
  local url="$1" dest="$2"
  run mkdir -p "$(dirname "$dest")"
  if [ "$DRY_RUN" -eq 1 ]; then
    printf '[dry-run] curl -fsSL %s -o %s\n' "$url" "$dest"
    return 0
  fi
  curl -fsSL --retry 2 --connect-timeout 15 "$url" -o "$dest"
}

if [ -n "$ARCHIVE" ]; then
  if [ -d "$ARCHIVE" ]; then
    SRC="$ARCHIVE/$ASSET"
    SUMS="$ARCHIVE/SHA256SUMS"
  else
    SRC="$ARCHIVE"
    SUMS="$(dirname "$ARCHIVE")/SHA256SUMS"
  fi
  if [ ! -f "$SRC" ]; then
    printf 'archive not found: %s\n' "$SRC" >&2
    exit 2
  fi
  run mkdir -p "$CACHE_DIR"
  run cp "$SRC" "$CACHE_DIR/$ASSET"
  verify_checksum "$CACHE_DIR/$ASSET" "$SUMS"
else
  SUMS_URL="${GITHUB_BASE}/download/v${VERSION}/SHA256SUMS"
  ASSET_URLS=("${GITHUB_BASE}/download/v${VERSION}/${ASSET}")
  if [ -n "$ATOMGIT_BASE" ]; then
    ASSET_URLS+=("${ATOMGIT_BASE%/}/download/v${VERSION}/${ASSET}")
  fi
  LAST_ERROR=""
  for url in "${ASSET_URLS[@]}"; do
    if fetch "$url" "$CACHE_DIR/$ASSET"; then
      LAST_ERROR=""
      break
    fi
    LAST_ERROR="$url"
  done
  if [ -n "$LAST_ERROR" ]; then
    printf 'download failed: %s\n' "$LAST_ERROR" >&2
    printf 'use --archive with a local release asset, or set GDA_ATOMGIT_BASE\n' >&2
    exit 1
  fi
  if fetch "$SUMS_URL" "$CACHE_DIR/SHA256SUMS" 2>/dev/null; then
    verify_checksum "$CACHE_DIR/$ASSET" "$CACHE_DIR/SHA256SUMS"
  else
    printf 'warning: SHA256SUMS unavailable; skipping checksum\n' >&2
  fi
fi

UNPACK="${CACHE_DIR}/unpacked"
run rm -rf "$UNPACK"
run mkdir -p "$UNPACK"
if [ "$EXT" = "zip" ]; then
  if ! run unzip -q -o "$CACHE_DIR/$ASSET" -d "$UNPACK"; then
    run tar -xf "$CACHE_DIR/$ASSET" -C "$UNPACK"
  fi
else
  run tar -xzf "$CACHE_DIR/$ASSET" -C "$UNPACK"
fi

BIN=""
for candidate in "$UNPACK/gda" "$UNPACK/gda.exe" "$UNPACK/bin/gda" "$UNPACK/bin/gda.exe"; do
  if [ -f "$candidate" ]; then BIN="$candidate"; break; fi
done
if [ -z "$BIN" ]; then
  printf 'error: no gda binary in %s\n' "$UNPACK" >&2
  exit 1
fi

run mkdir -p "$PREFIX"
TARGET="$PREFIX/gda"
TMP="${TARGET}.new-$$"
run cp "$BIN" "$TMP"
run chmod +x "$TMP"
if [ -f "$TARGET" ]; then
  run mv "$TARGET" "${TARGET}.old-$(date +%s)"
fi
run mv "$TMP" "$TARGET"

if [ "$NO_PATH" -eq 0 ]; then
  case ":$PATH:" in
    *":$PREFIX:"*) ;;
    *)
      case "${SHELL:-}" in
        *zsh*) PROFILE="$HOME/.zshrc" ;;
        *bash*) PROFILE="$HOME/.bashrc" ;;
        *) PROFILE="$HOME/.profile" ;;
      esac
      if [ "$DRY_RUN" -eq 1 ]; then
        printf '[dry-run] append PATH to %s\n' "$PROFILE"
      else
        printf '\n# Godot Autopilot CLI\nexport PATH="%s:$PATH"\n' "$PREFIX" >> "$PROFILE"
      fi
      ;;
  esac
fi

printf 'installed gda %s to %s\n' "$VERSION" "$TARGET"
printf 'next: %s/bin/gda install skills\n' "$HOME/.godot-autopilot"
printf 'next: %s/bin/gda install plugin --project <project>\n' "$HOME/.godot-autopilot"
