# Repository Guidelines

## Project Structure & Module Organization

Core C++17 files live in `src/`; public C-compatible headers are under `include/repl/`. Executable entry points are in `examples/skred/` and `examples/demo/`, CMake support is in `cmake/`, and sample panel data stays at the repository root. Treat `build/` and downloaded `vendor/` contents as generated. `third_party/pikchr/` is a pinned, committed dependency; keep its generated upstream sources unchanged.

## Build, Test, and Development Commands

- `make` configures a Release build and compiles `build/skrepl` or the macOS `build/skrepl.app` bundle.
- `make run` rebuilds and launches the FLTK application.
- `cmake -S . -B build -DSKRED_VERSION=0.52.0` selects a packaged Skred release version.
- `cmake --build build -j` performs an incremental parallel build.
- `ctest --test-dir build --output-on-failure` runs headless SVG, topology, spectrogram, and waveform tests.
- `./build/skrepl --check` verifies the linked release without opening a window or starting audio; on macOS use `./build/skrepl.app/Contents/MacOS/skrepl --check`.
- `make info` prints relevant cached CMake configuration; `make clean` removes the build directory.

Initial configuration may download FLTK and Skred. Offline builds can set `-DREPL_FLTK_DIR=/path/to/fltk` or populate `third_party/fltk`.
On macOS the executable target is emitted as `build/skrepl.app`, bundles
Skred dylibs, and is ad-hoc signed unless `FLTK_REPL_ADHOC_SIGN=OFF`.

## Coding Style & Naming Conventions

Use four-space indentation and same-line braces. C++ types use `PascalCase` (`TerminalView`), methods and locals use `camelCase`, and C API symbols use prefixes such as `repl_` with `snake_case`. Preserve C99-compatible public APIs and `extern "C"` guards. No formatter or linter is configured; avoid unrelated whitespace churn.

## Testing Guidelines

There is no coverage threshold. Every change should complete a clean build, pass CTest, and pass the platform-appropriate `--check` command above. Tests cover headless in-memory SVG parsing, Pikchr voice-graph conversion, and float-audio spectrogram and waveform rendering. For terminal, topology, bitmap, panel, theme, font, history, or clipboard changes, also run `make run` and manually exercise the affected interaction. Add tests under `tests/` and register them with CTest.

## Commit & Pull Request Guidelines

Recent history uses short, imperative, lowercase subjects. Make them specific and keep commits focused. Pull requests should explain user-visible changes, list test results, note platform behavior, and link issues. Include screenshots for visible FLTK changes.
