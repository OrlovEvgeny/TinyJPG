#!/bin/sh
set -e

MANIFEST_URL="https://tinyjpg.eorlov.org/tinyjpg/manifest.json"
tmpdir=""
archive=""

cleanup() {
  if [ -n "$tmpdir" ] && [ -d "$tmpdir" ]; then
    rm -rf "$tmpdir"
  fi
  if [ -n "$archive" ] && [ -f "$archive" ]; then
    rm -f "$archive"
  fi
}

die() {
  echo "error: $*" >&2
  exit 1
}

trap cleanup EXIT INT TERM

VERSION_ARG=""
if [ "$#" -gt 0 ]; then
  case "$1" in
    --version)
      [ "$#" -ge 2 ] || die "--version requires a value"
      VERSION_ARG="$2"
      shift 2
      ;;
    *)
      die "unsupported argument: $1"
      ;;
  esac
fi

[ "$#" -eq 0 ] || die "too many arguments"
command -v curl >/dev/null 2>&1 || die "curl is required"
command -v tar >/dev/null 2>&1 || die "tar is required"

os=$(uname -s 2>/dev/null || true)
machine=$(uname -m 2>/dev/null || true)

case "$machine" in
  x86_64|amd64|AMD64) arch="x86_64" ;;
  aarch64|arm64) arch="aarch64" ;;
  *) die "unsupported architecture: $machine" ;;
esac

case "$os" in
  Linux)
    platform="linux-$arch"
    ;;
  Darwin)
    if [ "$arch" = "x86_64" ]; then
      die "macOS x86_64 is not supported, use Apple Silicon"
    fi
    platform="macos-arm64"
    ;;
  *)
    die "unsupported operating system: $os"
    ;;
esac

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/tinyjpg.XXXXXX") || die "failed to create temporary directory"
manifest="$tmpdir/manifest.json"
version_manifest="$tmpdir/version.json"
archive="$tmpdir/tinyjpg.tar.gz"

echo "Fetching TinyJPG manifest..."
curl -fsSL "$MANIFEST_URL" -o "$manifest" || die "failed to download manifest from $MANIFEST_URL"

json_latest() {
  if command -v jq >/dev/null 2>&1; then
    jq -r '.latest' "$manifest"
  else
    grep '"latest"' "$manifest" | sed 's/.*: *"//' | sed 's/".*//' | sed -n '1p'
  fi
}

json_platform_field() {
  field="$1"
  if command -v jq >/dev/null 2>&1; then
    jq -r --arg version "$version" --arg platform "$platform" --arg field "$field" \
      '.versions[$version].platforms[$platform][$field] // empty' "$version_manifest"
  else
    awk -v version="$version" -v platform="$platform" -v field="$field" '
      function count_chars(text, ch, n, i) {
        n = 0
        for (i = 1; i <= length(text); i++) {
          if (substr(text, i, 1) == ch) {
            n++
          }
        }
        return n
      }
      $0 ~ "\"" version "\"" "[[:space:]]*:" {
        in_version = 1
        depth += count_chars($0, "{") - count_chars($0, "}")
        next
      }
      in_version && $0 ~ "\"" platform "\"" "[[:space:]]*:" { in_platform = 1 }
      in_platform && $0 ~ "\"" field "\"" "[[:space:]]*:" {
        sub("^[^\"]*\"" field "\"[[:space:]]*:[[:space:]]*\"", "")
        sub("\".*$", "")
        print
        exit
      }
      in_version {
        depth += count_chars($0, "{") - count_chars($0, "}")
        if (depth <= 0) {
          exit
        }
      }
    ' "$version_manifest"
  fi
}

if [ -n "$VERSION_ARG" ]; then
  version="$VERSION_ARG"
else
  version=$(json_latest)
fi

[ -n "$version" ] || die "failed to determine TinyJPG version from manifest"

echo "Fetching TinyJPG $version metadata..."
curl -fsSL "https://tinyjpg.eorlov.org/tinyjpg/$version/manifest.json" -o "$version_manifest" \
  || die "failed to download version manifest for $version"

url=$(json_platform_field url)
expected_sha=$(json_platform_field sha256)

[ -n "$url" ] || die "no download URL found for platform $platform in TinyJPG $version manifest"
[ -n "$expected_sha" ] || die "no SHA256 found for platform $platform in TinyJPG $version manifest"

echo "Downloading TinyJPG $version for $platform..."
curl -fsSL "$url" -o "$archive" || die "failed to download $url"

if command -v sha256sum >/dev/null 2>&1; then
  actual_sha=$(sha256sum "$archive" | awk '{print $1}')
elif command -v shasum >/dev/null 2>&1; then
  actual_sha=$(shasum -a 256 "$archive" | awk '{print $1}')
else
  die "sha256sum or shasum is required to verify downloads"
fi

[ "$actual_sha" = "$expected_sha" ] || die "SHA256 mismatch! Expected $expected_sha, got $actual_sha"

if [ -n "${INSTALL_DIR:-}" ]; then
  install_dir="$INSTALL_DIR"
elif [ "$(id -u)" = "0" ]; then
  install_dir="/usr/local/bin"
else
  install_dir="$HOME/.local/bin"
fi

echo "Installing to $install_dir"
mkdir -p "$install_dir" || die "failed to create install directory: $install_dir"

extract_dir="$tmpdir/extracted"
mkdir -p "$extract_dir" || die "failed to create extraction directory"
tar xzf "$archive" -C "$extract_dir" || die "failed to extract archive"

[ -f "$extract_dir/tinyjpg" ] || die "archive did not contain tinyjpg"
[ -f "$extract_dir/tj" ] || die "archive did not contain tj"

cp "$extract_dir/tinyjpg" "$install_dir/tinyjpg" || die "failed to install tinyjpg to $install_dir"
cp "$extract_dir/tj" "$install_dir/tj" || die "failed to install tj to $install_dir"
chmod +x "$install_dir/tinyjpg" "$install_dir/tj" || die "failed to mark binaries executable"

"$install_dir/tinyjpg" --version >/dev/null || die "installed tinyjpg failed verification"

case ":$PATH:" in
  *":$install_dir:"*) ;;
  *)
    if [ "$install_dir" = "$HOME/.local/bin" ]; then
      echo "Add ~/.local/bin to your PATH:"
      echo '  export PATH="$HOME/.local/bin:$PATH"'
    fi
    ;;
esac

echo "TinyJPG $version installed successfully!"
