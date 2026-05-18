<p align="center">
  <img src="https://raw.githubusercontent.com/OrlovEvgeny/TinyJPG/master/doc/logo.png" width="340" alt="TinyJPG">
</p>

<p align="center">
  <a href="https://github.com/OrlovEvgeny/TinyJPG/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/OrlovEvgeny/TinyJPG/actions/workflows/ci.yml/badge.svg?branch=feature/cpp-rewrite"></a>
  <img alt="C++23" src="https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white">
  <img alt="CMake" src="https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white">
  <img alt="vcpkg" src="https://img.shields.io/badge/deps-vcpkg-2F74C0">
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/badge/license-MIT-green.svg"></a>
</p>

# TinyJPG

TinyJPG is a C++23 image optimizer built for fast, repeatable asset pipelines. It
compresses JPEG, PNG, WebP, AVIF, and JPEG XL files in-process, generates
responsive variants, scans folders, watches upload directories, and reports
results as text, tables, or JSON.

The executable is available as both `tinyjpg` and the short alias `tj`.

## Preview

| Before | After |
| --- | --- |
| ![Original portrait before compression](doc/meg-before.jpg) | ![Portrait after TinyJPG compression](doc/meg-after.jpg) |
| 1.5 MB JPEG | 1.3 MB JPEG |

This sample keeps the original 2236 x 1792 dimensions and reduces the file by
about 16% with a lossless JPEG repack, so the optimized image decodes to the
same pixels as the source.

## Why TinyJPG

- Native C++23 command line tool with no shell-outs for image conversion.
- Codec coverage for JPEG, PNG, WebP, AVIF, and JPEG XL workflows.
- One-shot `run`, recursive `scan`, and continuous `watch` modes.
- Built-in responsive presets plus TOML configuration for custom variants.
- Atomic writes, dry-run planning, skip-if-not-smaller behavior, and structured
  output for CI and automation.
- Cross-platform CMake and vcpkg build on Linux, macOS, and Windows.

## Install

Build from source with CMake, Ninja, a C++23 compiler, and vcpkg:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix ~/.local
```

Create local packages with CPack:

```bash
cpack --config build/CPackConfig.cmake
```

## Usage

Optimize explicit files:

```bash
tj run image.jpg image.png --config tinyjpg.toml --format table
```

Scan files and directories:

```bash
tj scan ./images --preset web --dry-run --format json
```

Watch a directory for new or changed images:

```bash
tj watch ./uploads --config tinyjpg.toml
```

Inspect built-in responsive presets:

```bash
tj presets list
```

Generate shell completion:

```bash
tj completion zsh > _tj
```

## Configuration

Generate a default TOML config:

```bash
tj config print --defaults > tinyjpg.toml
```

Validate it before using it in automation:

```bash
tj config validate tinyjpg.toml
```

Example config:

```toml
[general]
workers = 0
log_level = "info"
queue_capacity = 512
stable_wait_ms = 400
dry_run = false

[watch]
paths = ["./uploads"]
recursive = true
include = ["*.jpg", "*.jpeg", "*.png", "*.webp"]
exclude = ["**/.cache/**", "*.tmp"]
prefix = []

[compress]
mode = "lossless"
effort = "max"
keep_metadata = false
skip_if_not_smaller = true
preserve_original = true

[[variant]]
name = "medium"
codec = "auto"
max_width = 1024
quality = 82
suffix = "-medium"

[output]
pattern = "{dir}/{stem}{suffix}.{ext}"
directory = ""
on_exist = "skip"
```

## Service Assets

Install packages include service helpers for long-running optimization:

- systemd unit, sysusers, and tmpfiles snippets for Linux.
- launchd plist template for macOS.
- PowerShell install and uninstall scripts for Windows services.

## License

[MIT](LICENSE)
