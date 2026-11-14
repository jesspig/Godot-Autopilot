#!/usr/bin/env bash
set -euo pipefail

VERSION=""
PREFIX="${HOME}/.godot-autopilot/bin"
ARCHIVE=""
PROJECT=""
PROJECT_SOURCE=""
PROJECT_EXPLICIT=0
SKILLS_MODE=""
ZH=0
NO_PATH=0
NO_PLUGIN=0
NO_VERIFY=0
DRY_RUN=0
GITHUB_BASE="${GDA_GITHUB_BASE:-https://github.com/jesspig/Godot-Autopilot/releases}"

usage() {
  printf 'usage: install.sh [--version V] [--prefix DIR] [--archive PATH] [--project DIR]\n'
  printf '                  [--skills | --skills-project] [--zh] [--no-path] [--no-plugin]\n'
  printf '                  [--skip-checksum] [--dry-run]\n'
}

fail() {
  printf 'error: %s\n' "$*" >&2
  exit 2
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --version) VERSION="$2"; shift 2 ;;
    --prefix) PREFIX="$2"; shift 2 ;;
    --archive) ARCHIVE="$2"; shift 2 ;;
    --project) PROJECT="$2"; PROJECT_EXPLICIT=1; shift 2 ;;
    --skills)
      [ "$SKILLS_MODE" = "project" ] && fail "--skills and --skills-project are mutually exclusive"
      SKILLS_MODE="global"
      shift
      ;;
    --skills-project)
      [ "$SKILLS_MODE" = "global" ] && fail "--skills and --skills-project are mutually exclusive"
      SKILLS_MODE="project"
      shift
      ;;
    --zh) ZH=1; shift ;;
    --no-path) NO_PATH=1; shift ;;
    --no-plugin) NO_PLUGIN=1; shift ;;
    --skip-checksum) NO_VERIFY=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) fail "unknown option: $1" ;;
  esac
done

if [ "$ZH" -eq 1 ] && [ -z "$SKILLS_MODE" ]; then
  fail "--zh requires --skills or --skills-project"
fi

run() {
  if [ "$DRY_RUN" -eq 1 ]; then
    printf '[dry-run] %s\n' "$*"
  else
    "$@"
  fi
}

resolve_latest() {
  local tag
  tag="$(curl -fsSL --connect-timeout 15 \
    "https://api.github.com/repos/jesspig/Godot-Autopilot/releases/latest" \
    | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n1)"
  if [ -z "$tag" ]; then
    fail "cannot resolve the latest release; pass --version <version>"
  fi
  printf '%s\n' "$tag"
}

normalize_dir() {
  ( cd "$1" 2>/dev/null && pwd )
}

find_project_root() {
  local dir="$1"
  while :; do
    if [ -f "$dir/project.godot" ]; then
      printf '%s\n' "$dir"
      return 0
    fi
    local parent
    parent="$(dirname "$dir")"
    if [ "$parent" = "$dir" ]; then
      return 1
    fi
    dir="$parent"
  done
}

if [ -z "$PROJECT" ] && [ -n "${GDA_PROJECT:-}" ]; then
  PROJECT="$GDA_PROJECT"
  PROJECT_EXPLICIT=1
fi

if [ "$PROJECT_EXPLICIT" -eq 1 ]; then
  PROJECT_INPUT="$PROJECT"
  PROJECT="$(normalize_dir "$PROJECT" || true)"
  if [ -z "$PROJECT" ] || [ ! -f "$PROJECT/project.godot" ]; then
    fail "not a Godot project (project.godot missing): $PROJECT_INPUT"
  fi
  PROJECT_SOURCE="explicit"
elif [ "$NO_PLUGIN" -eq 0 ] || [ "$SKILLS_MODE" = "project" ]; then
  if [ -f "$PWD/project.godot" ]; then
    PROJECT="$PWD"
    PROJECT_SOURCE="cwd"
  elif PROJECT="$(find_project_root "$PWD")"; then
    PROJECT_SOURCE="parent"
  else
    PROJECT=""
  fi
fi

if [ "$SKILLS_MODE" = "project" ] && [ -z "$PROJECT" ]; then
  fail "--skills-project requires a Godot project; pass --project <dir>"
fi

if [ -z "$VERSION" ]; then
  VERSION="${GDA_VERSION:-}"
fi
if [ -z "$VERSION" ]; then
  if [ -n "$ARCHIVE" ]; then
    if [ -d "$ARCHIVE" ] && [ -f "$ARCHIVE/VERSION" ]; then
      VERSION="$(tr -d ' \t\r\n' < "$ARCHIVE/VERSION")"
    else
      fail "a version is required with --archive (--version or GDA_VERSION)"
    fi
  else
    VERSION="$(resolve_latest)"
  fi
fi
VERSION="${VERSION#v}"

detect_platform() {
  os="$(uname -s)"
  arch="$(uname -m)"
  case "$os" in
    Darwin)
      case "$arch" in
        arm64) echo "macos-arm64 tar.gz" ;;
        x86_64) echo "macos-x64 tar.gz" ;;
        *) fail "unsupported macOS arch: $arch" ;;
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
      fail "unsupported OS: $os"
      ;;
  esac
}

read -r PLATFORM EXT <<<"$(detect_platform)"
ASSET="gda-${VERSION}-${PLATFORM}.${EXT}"
CACHE_DIR="${HOME}/.godot-autopilot/cache/cli/${VERSION}/${PLATFORM}"

verify_checksum() {
  local file="$1" sums="$2" expected actual
  if [ ! -f "$sums" ]; then
    if [ "$NO_VERIFY" -eq 1 ]; then
      printf 'warning: checksum file missing; skipping checksum\n' >&2
      return 0
    fi
    fail "checksum file missing: $sums"
  fi
  expected="$(grep "  ${ASSET}$" "$sums" | awk '{print $1}' || true)"
  if [ -z "$expected" ]; then
    if [ "$NO_VERIFY" -eq 1 ]; then
      printf 'warning: %s is not listed in checksums; skipping\n' "$ASSET" >&2
      return 0
    fi
    fail "$ASSET is not listed in $sums"
  fi
  if command -v sha256sum >/dev/null 2>&1; then
    actual="$(sha256sum "$file" | awk '{print $1}')"
  else
    actual="$(shasum -a 256 "$file" | awk '{print $1}')"
  fi
  if [ "$expected" != "$actual" ]; then
    fail "checksum mismatch for $ASSET"
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
    SUMS="$ARCHIVE/sha256sums.txt"
  else
    SRC="$ARCHIVE"
    SUMS="$(dirname "$ARCHIVE")/sha256sums.txt"
  fi
  if [ ! -f "$SRC" ]; then
    fail "archive not found: $SRC"
  fi
  run mkdir -p "$CACHE_DIR"
  run cp "$SRC" "$CACHE_DIR/$ASSET"
  if [ "$DRY_RUN" -eq 0 ]; then
    verify_checksum "$CACHE_DIR/$ASSET" "$SUMS"
  fi
else
  ASSET_TAG=""
  for tag in "v${VERSION}" "${VERSION}"; do
    if fetch "${GITHUB_BASE}/download/${tag}/${ASSET}" "$CACHE_DIR/$ASSET"; then
      ASSET_TAG="$tag"
      break
    fi
  done
  if [ -z "$ASSET_TAG" ]; then
    fail "download failed for gda ${VERSION} (${PLATFORM}); use --archive with a local release asset"
  fi
  SUMS_URL="${GITHUB_BASE}/download/${ASSET_TAG}/sha256sums.txt"
  if fetch "$SUMS_URL" "$CACHE_DIR/sha256sums.txt"; then
    if [ "$DRY_RUN" -eq 0 ]; then
      verify_checksum "$CACHE_DIR/$ASSET" "$CACHE_DIR/sha256sums.txt"
    fi
  elif [ "$NO_VERIFY" -eq 1 ]; then
    printf 'warning: %s unavailable; skipping checksum\n' "$SUMS_URL" >&2
  else
    fail "cannot download $SUMS_URL; pass --skip-checksum to install without verification"
  fi
fi

if [ "$DRY_RUN" -eq 1 ]; then
  ZH_SUFFIX=""
  if [ "$ZH" -eq 1 ]; then ZH_SUFFIX=" --zh"; fi
  printf '[dry-run] install gda %s to %s\n' "$VERSION" "$PREFIX"
  if [ -n "$PROJECT" ] && [ "$NO_PLUGIN" -eq 0 ]; then
    printf '[dry-run] %s install plugin --project %s\n' "$PREFIX/gda" "$PROJECT"
  else
    printf '[dry-run] plugin skipped\n'
  fi
  if [ "$SKILLS_MODE" = "global" ]; then
    printf '[dry-run] %s install skills --force%s\n' "$PREFIX/gda" "$ZH_SUFFIX"
  elif [ "$SKILLS_MODE" = "project" ]; then
    printf '[dry-run] %s install skills --path %s --force%s\n' "$PREFIX/gda" "$PROJECT/.agents/skills" "$ZH_SUFFIX"
  else
    printf '[dry-run] skills skipped\n'
  fi
  exit 0
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
  fail "no gda binary in $UNPACK"
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
if [ -n "$PROJECT" ]; then
  printf 'project: %s (%s)\n' "$PROJECT" "$PROJECT_SOURCE"
fi

if [ -n "$PROJECT" ] && [ "$NO_PLUGIN" -eq 0 ]; then
  run "$TARGET" install plugin --project "$PROJECT"
  if [ "$DRY_RUN" -eq 0 ]; then
    if [ ! -f "$PROJECT/addons/godot-autopilot/godot-autopilot.gdextension" ]; then
      fail "plugin install did not produce addons/godot-autopilot/godot-autopilot.gdextension"
    fi
    printf 'installed plugin to %s\n' "$PROJECT/addons/godot-autopilot"
  fi
elif [ -n "$PROJECT" ]; then
  printf 'plugin: skipped (--no-plugin)\n'
else
  printf 'plugin: skipped (no project.godot found; pass --project <dir>)\n'
fi

if [ -n "$SKILLS_MODE" ]; then
  SKILL_ARGS=(install skills --force)
  if [ "$ZH" -eq 1 ]; then SKILL_ARGS+=(--zh); fi
  if [ "$SKILLS_MODE" = "project" ]; then
    SKILLS_DIR="$PROJECT/.agents/skills"
    SKILL_ARGS+=(--path "$SKILLS_DIR")
  else
    SKILLS_DIR="$HOME/.agents/skills"
  fi
  run "$TARGET" "${SKILL_ARGS[@]}"
  if [ "$DRY_RUN" -eq 0 ]; then
    if [ ! -f "$SKILLS_DIR/godot-autopilot/SKILL.md" ]; then
      fail "skills install did not produce $SKILLS_DIR/godot-autopilot/SKILL.md"
    fi
    printf 'installed skills to %s\n' "$SKILLS_DIR"
  fi
else
  printf 'skills: skipped\n'
fi

printf 'next: %s --version\n' "$TARGET"
printf 'next: %s start --project <project>\n' "$TARGET"
printf 'next: %s status --json\n' "$TARGET"
