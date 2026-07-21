# Repository Guidelines

## Project Structure & Module Organization

Core C++17 implementation files live in `src/`; public, C-compatible headers are under `include/repl/`. Keep declarations intended for library consumers in the public include tree and implementation-only classes beside their `.cpp` files in `src/`. Executable entry points are in `examples/skred/` and `examples/demo/`. CMake support belongs in `cmake/`, while sample panel data such as `controls.pnl` stays at the repository root. Treat `build/` and downloaded `vendor/` contents as generated artifacts; do not edit or commit them.

## Build, Test, and Development Commands

- `make` configures a Release build and compiles `build/skred_repl`.
- `make run` rebuilds and launches the FLTK application.
- `cmake -S . -B build -DSKRED_VERSION=0.51.0` selects a packaged Skred release version.
- `cmake --build build -j` performs an incremental parallel build.
- `ctest --test-dir build --output-on-failure` runs the headless SVG smoke test.
- `./build/skred_repl --check` verifies the linked release without opening a window or starting audio.
- `make info` prints relevant cached CMake configuration; `make clean` removes the build directory.

Initial configuration may download FLTK and Skred. For offline FLTK builds, provide `-DREPL_FLTK_DIR=/path/to/fltk` or populate `third_party/fltk`.

## Coding Style & Naming Conventions

Match nearby code: four-space indentation, braces on the same line, and focused comments explaining behavior rather than syntax. C++ types use `PascalCase` (`TerminalView`), methods and local variables use `camelCase`, and C API symbols use the `repl_`, `bitmap_win_`, or `panel_` prefixes with `snake_case`. Preserve the C99-compatible public API and `extern "C"` guards. The project has no configured formatter or linter, so avoid unrelated whitespace churn and compile with warnings clean where practical.

## Testing Guidelines

There is no coverage threshold. Every change should complete a clean build, pass CTest, and pass `./build/skred_repl --check`. The `tests/svg_smoke.cpp` test validates in-memory SVG parsing without opening a window. For terminal, bitmap, panel, theme, font, history, or clipboard changes, also run `make run` and manually exercise the affected interaction. Add tests under `tests/` and register them with CTest.

## Commit & Pull Request Guidelines

Recent history uses short, imperative, lowercase subjects (for example, `untangle` and `integrate new features`). Prefer similarly concise subjects, but make them specific to the affected behavior. Keep commits focused. Pull requests should explain the user-visible change, list build and manual test results, note platform-specific behavior, and link relevant issues. Include screenshots for visible FLTK, bitmap, or panel changes.
