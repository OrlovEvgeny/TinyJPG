<p align="center">
  <img src="doc/tj_logo.png" width="260" alt="TinyJPG logo">
</p>

<p align="center">
  <a href="https://github.com/OrlovEvgeny/TinyJPG/actions/workflows/ci.yml"><img alt="CI status" src="https://img.shields.io/github/actions/workflow/status/OrlovEvgeny/TinyJPG/ci.yml?branch=feature%2Fcpp-rewrite&style=for-the-badge&logo=githubactions&logoColor=white&label=CI"></a>
  <a href="https://github.com/OrlovEvgeny/TinyJPG/releases"><img alt="Releases" src="https://img.shields.io/badge/release-R2%20publishing-0EA5E9?style=for-the-badge&logo=githubactions&logoColor=white"></a>
  <a href="#install"><img alt="Install TinyJPG" src="https://img.shields.io/badge/install-curl%20%7C%20brew%20%7C%20powershell-16A34A?style=for-the-badge&logo=icloud&logoColor=white"></a>
  <a href="https://en.cppreference.com/w/cpp/compiler_support/23"><img alt="C++23" src="https://img.shields.io/badge/C%2B%2B-23-00599C?style=for-the-badge&logo=cplusplus&logoColor=white"></a>
  <a href="https://cmake.org/"><img alt="CMake" src="https://img.shields.io/badge/build-CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white"></a>
  <a href="https://learn.microsoft.com/vcpkg/"><img alt="vcpkg" src="https://img.shields.io/badge/deps-vcpkg-2F74C0?style=for-the-badge"></a>
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/badge/license-MIT-22C55E?style=for-the-badge"></a>
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
| 1.5 MB JPEG | 243 KB JPEG |

This sample keeps the original 2236 x 1792 dimensions and reduces the file by
about 84%.

## Why TinyJPG

- Native C++23 command line tool with no shell-outs for image conversion.
- Codec coverage for JPEG, PNG, WebP, AVIF, and JPEG XL workflows.
- One-shot `run`, recursive `scan`, and continuous `watch` modes.
- Built-in responsive presets plus TOML configuration for custom variants.
- Atomic writes, dry-run planning, skip-if-not-smaller behavior, and structured
  output for CI and automation.
- Cross-platform CMake and vcpkg build on Linux, macOS, and Windows.

## Install

Install the latest release on Linux or macOS:

```bash
curl -fsSL https://tj.eorlov.org/install.sh | sh
```

Install the latest release on Windows PowerShell:

```powershell
irm https://pq.eorlov.org/install.ps1 | iex
```

Install with Homebrew on Apple Silicon macOS:

```bash
brew tap OrlovEvgeny/tinyjpg
brew install tinyjpg
```

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

### Practical config

This example watches an upload directory, writes all generated files into a
separate output directory, and creates three variants for web delivery:

```toml
[general]
workers = 0
log_level = "info"
queue_capacity = 512
stable_wait_ms = 400
dry_run = false

[watch]
paths = ["/var/lib/tinyjpg/inbox"]
recursive = true
include = ["*.jpg", "*.jpeg", "*.png", "*.webp"]
exclude = ["**/.cache/**", "*.tmp"]
prefix = []

[compress]
mode = "visually_lossless"
effort = "balanced"
keep_metadata = false
skip_if_not_smaller = true
preserve_original = true

[[variant]]
name = "large"
codec = "auto"
max_width = 1920
suffix = "-large"

[[variant]]
name = "card"
codec = "webp"
mode = "lossy"
max_width = 960
quality = 82
suffix = "-card"

[[variant]]
name = "thumb"
codec = "auto"
max_width = 320
max_height = 320
quality = 76
fit = "cover"
suffix = "-thumb"

[output]
directory = "/var/lib/tinyjpg/output"
pattern = "{stem}{suffix}.{ext}"
on_exist = "version"
```

With this config, `/var/lib/tinyjpg/inbox/hero.jpg` can produce:

```text
/var/lib/tinyjpg/output/hero-large.jpg
/var/lib/tinyjpg/output/hero-card.webp
/var/lib/tinyjpg/output/hero-thumb.jpg
```

### Config reference

`[general]`

| Setting | Values | Meaning |
| --- | --- | --- |
| `workers` | `0` or positive integer | Number of worker threads. `0` uses hardware concurrency. |
| `log_level` | `trace`, `debug`, `info`, `warn`, `error` | Runtime verbosity. |
| `queue_capacity` | positive integer | Maximum pending work items before producers wait. |
| `stable_wait_ms` | `0` or positive integer | Delay used by watch mode so files finish writing before processing. |
| `dry_run` | `true`, `false` | Plan work without writing output files. Can also be set with `--dry-run`. |

`[watch]`

| Setting | Values | Meaning |
| --- | --- | --- |
| `paths` | array of paths | Default paths for `tj watch` when no CLI path is passed. |
| `recursive` | `true`, `false` | Parsed from TOML; directory scanning currently walks recursively. |
| `include` | glob array | Parsed from TOML; current processing still selects files by supported image codec. |
| `exclude` | glob array | Parsed from TOML; generated TinyJPG variants are skipped automatically. |
| `prefix` | string array | Parsed from TOML and reserved for path-prefix filtering. |

`[compress]`

| Setting | Values | Meaning |
| --- | --- | --- |
| `mode` | `lossless`, `visually_lossless`, `lossy` | Default fidelity target for variants. |
| `effort` | `fast`, `balanced`, `max` | Encoder effort/speed preference. |
| `keep_metadata` | `true`, `false` | Keep image metadata when possible. |
| `skip_if_not_smaller` | `true`, `false` | Do not replace/write variants that are larger than the source. |
| `preserve_original` | `true`, `false` | Avoid overwriting the original path; an empty suffix becomes `-optimized`. |

`[[variant]]`

Each variant describes one output image. At least one variant is required.
Variant names may contain letters, digits, `_`, and `-`.

| Setting | Values | Meaning |
| --- | --- | --- |
| `name` | string | Variant name shown in output and used by `{name}`. |
| `codec` | `auto`, `jpeg`, `png`, `webp`, `avif`, `jxl` | Output codec. `auto` keeps the source codec. |
| `mode` | same as `[compress].mode` | Optional per-variant fidelity override. |
| `quality` | `1` to `100` | Encoder quality. If omitted, TinyJPG uses `82`. |
| `max_width` | positive integer | Maximum output width. |
| `max_height` | positive integer | Maximum output height. |
| `fit` | `contain`, `cover`, `fill` | Resize behavior when both dimensions are set. |
| `suffix` | string | Added to output file names, for example `-thumb`. |

Every non-`original` variant must set `max_width`, `max_height`, or both.
The special variant name `original` is allowed without size constraints.

`[output]`

| Setting | Values | Meaning |
| --- | --- | --- |
| `directory` | path or empty string | Output directory. Empty means next to the input file. |
| `pattern` | string with tokens | Output path template. |
| `on_exist` | `skip`, `overwrite`, `version` | Behavior when the output path already exists. |

Supported `pattern` tokens:

| Token | Value |
| --- | --- |
| `{dir}` | Input directory or configured output directory. |
| `{stem}` | Input file name without extension. |
| `{suffix}` | Variant suffix. |
| `{ext}` | Output extension for the chosen codec. |
| `{name}` | Variant name. |
| `{codec}` | Output codec name. |
| `{width}` | Planned output width. |
| `{height}` | Planned output height. |

Useful patterns:

```toml
pattern = "{dir}/{stem}{suffix}.{ext}"
pattern = "{stem}-{width}w.{ext}"
pattern = "{codec}/{stem}-{name}.{ext}"
```

### Presets

You can replace configured variants with a built-in preset at runtime:

```bash
tj scan ./images --preset web
tj scan ./products --preset ecommerce
tj scan ./avatars --preset avatar
```

Available presets:

| Preset | Variants |
| --- | --- |
| `web` | `original`, `large` 1920w, `medium` 1024w, `thumb` 320x320 cover |
| `ecommerce` | `original`, `hero` 1600w, `listing` 900w, `thumb` 320w |
| `avatar` | `original`, `full` 512x512 cover, `thumb` 128x128 cover |

## Service Assets

Install packages include service helpers for long-running optimization:

- systemd unit, sysusers, and tmpfiles snippets for Linux.
- launchd plist template for macOS.
- PowerShell install and uninstall scripts for Windows services.

### systemd example

The generated unit runs:

```text
tinyjpg watch --config /etc/tinyjpg/tinyjpg.toml
```

It uses a dedicated `tinyjpg` user, protects the host filesystem, and only grants
write access to `/var/lib/tinyjpg` and `/var/log/tinyjpg`. Keep watched input and
output directories under `/var/lib/tinyjpg`, or add a systemd override with extra
`ReadWritePaths=`.

Example setup:

```bash
sudo systemd-sysusers packaging/systemd/tinyjpg.sysusers
sudo systemd-tmpfiles --create packaging/systemd/tinyjpg.tmpfiles

sudo install -d -m 0750 -o root -g root /etc/tinyjpg
sudo install -d -m 0750 -o tinyjpg -g tinyjpg /var/lib/tinyjpg/inbox
sudo install -d -m 0750 -o tinyjpg -g tinyjpg /var/lib/tinyjpg/output
sudo install -m 0640 -o root -g tinyjpg tinyjpg.toml /etc/tinyjpg/tinyjpg.toml
sudo install -m 0644 build/packaging/systemd/tinyjpg.service /etc/systemd/system/tinyjpg.service
sudo systemctl daemon-reload
sudo systemctl enable --now tinyjpg.service
```

Check service health and logs:

```bash
systemctl status tinyjpg.service
journalctl -u tinyjpg.service -f
```

If your images live outside `/var/lib/tinyjpg`, add an override:

```bash
sudo systemctl edit tinyjpg.service
```

```ini
[Service]
ReadWritePaths=/srv/uploads /srv/images /var/lib/tinyjpg /var/log/tinyjpg
```

Then reload and restart:

```bash
sudo systemctl daemon-reload
sudo systemctl restart tinyjpg.service
```

## License

[MIT](LICENSE)
