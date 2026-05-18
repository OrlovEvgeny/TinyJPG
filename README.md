<p align="center"><img src="https://raw.githubusercontent.com/OrlovEvgeny/TinyJPG/master/doc/logo.png" width="360"></p>

# TinyJPG

TinyJPG is a C++23 command line image optimizer for JPEG, PNG, WebP, AVIF, and JPEG XL workflows. It can optimize explicit files, scan directories, watch paths for changes, generate responsive variants, and emit text, table, or JSON output for automation.

## Features

- In-process image decoding and encoding with libjpeg-turbo, libpng, libwebp, libavif, and libjxl.
- `run`, `scan`, and `watch` commands for one-shot and continuous optimization.
- Built-in presets for common responsive image variants.
- TOML configuration with validation and default config generation.
- Dry-run mode, structured output, runtime diagnostics, and shell completions.

## Build

Install CMake, Ninja, a C++23 compiler, and vcpkg. Then configure with the vcpkg toolchain:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The binary is written to `build/tinyjpg`.

## Configuration

Generate a default TOML config:

```bash
tinyjpg config print --defaults > tinyjpg.toml
```

Validate it before using it in automation:

```bash
tinyjpg config validate tinyjpg.toml
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

## Usage

Optimize explicit files:

```bash
tinyjpg run image.jpg image.png --config tinyjpg.toml --format table
```

Scan files and directories:

```bash
tinyjpg scan ./images --preset web --dry-run --format json
```

Watch a directory:

```bash
tinyjpg watch ./uploads --config tinyjpg.toml
```

Inspect available presets and runtime support:

```bash
tinyjpg presets list
tinyjpg doctor --format table
```

Generate shell completion:

```bash
tinyjpg completion zsh > _tinyjpg
```

## License

[MIT](LICENSE)
