# FLTK REPL Library (`fltk-repl`)

A lightweight, embeddable, cross-platform C99/C++17 desktop GUI REPL terminal component built on FLTK 1.4+.

`fltk-repl` provides a complete, modern desktop GUI terminal with ANSI 256-color support, Unicode Braille pattern rendering (`U+2800..U+28FF`), customizable themes, font picking, interactive GUI parameter control panels (`panel_dsl`), standalone bitmap & audio visualization windows (`bitmap_win`), and thread-safe FFI bridges (`foreign_bridge`).

It serves both as:
1. **A Universal GUI REPL Library (`libreplfltk`)**: Embeddable desktop frontend for **Elixir**, **Forth**, **Python**, **Common Lisp**, **Lua**, **Scheme**, or **C/C++** interactive runtimes. (See [`docs/INTEGRATION.md`](docs/INTEGRATION.md) for foreign language binding guides).
2. **`skrepl`**: The desktop REPL application frontend for the Skred DSP audio synthesizer.

---

## Key Features

- **Pure C99 Public API (`include/repl/`)**: Clean, non-mangled C headers (`repl_api.h`, `panel_dsl.h`, `bitmap_win.h`, `foreign_bridge.h`). No C++ knowledge or compiler mangling required for host applications.
- **ANSI 256-Color & SGR Parser**: Native rendering of 16-color standard/bright and 256-color xterm terminal escape sequences (`\033[38;5;Nm`).
- **Unicode Braille Matrix Graphics**: Render 2x4 dot pattern sparklines, oscilloscope waveforms, and plots directly in text (`U+2800..U+28FF`).
- **Dynamic Parameter Control Panels (`panel_dsl`)**: Parse layout files (`.pnl`) to render interactive sliders, knobs, toggles, sub-panels, choices, and sequential button grids.
- **Bitmap & Audio Display Windows (`bitmap_win`)**: High-performance windows for RGB pixel buffers, audio waveforms, and log/linear spectrograms with spectral metric analysis.
- **Thread-Safe Foreign Bridge (`foreign_bridge`)**: Marshal input lines safely across thread boundaries (e.g., from Erlang/BEAM async tasks, Forth threads, or audio DSP loops) onto FLTK's event loop via `Fl::awake()`.
- **Desktop Features**: Built-in command history (Up/Down arrow recall), selection & clipboard support, clickable HTTP/HTTPS links, light/dark/custom themes, font size zooming, and native file dialogs across Linux, macOS, and Windows.

---

## Language Integration & Embedding

`fltk-repl` is designed for embedding in foreign programming languages and interactive runtimes. Detailed binding guides and code examples are available in [`docs/INTEGRATION.md`](docs/INTEGRATION.md):

- **[Elixir / Erlang (BEAM)](docs/INTEGRATION.md#3-elixir--erlang-beam-integration-guide)**: Connect Elixir interactive sessions to `fltk-repl` via Ports or NIFs.
- **[Forth (Gforth / VFX / C FFI)](docs/INTEGRATION.md#4-forth-integration-guide)**: Expose a desktop GUI console using C FFI (`c-function` / `C:`).
- **[Python (`ctypes` / `cffi`)](docs/INTEGRATION.md#5-python-integration-guide)**: Bind `libreplfltk` directly to execute Python statements and stream output.
- **[Embedded C / C++ Runtimes](docs/INTEGRATION.md#6-embedded-c--c-integration-guide)**: Embed `repl_ctx` into custom C/C++ DSLs or interpreters.

See [`docs/VENDORING.md`](docs/VENDORING.md) for complete instructions on vendoring `fltk-repl` via Git Submodules (`add_subdirectory`), building dynamic shared libraries (`libreplfltk.so`/`.dylib`/`.dll`), or setting up air-gapped offline builds.

---

## Quickstart & Build

The build system requires CMake 3.16 or newer and a C/C++ toolchain:

```sh
# Configure Release build
cmake -S . -B build

# Build all targets (libreplfltk, repl_demo, skrepl)
cmake --build build -j

# Run the generic standalone C REPL demo
./build/repl_demo

# Run the Skred synthesizer REPL executable
./build/skrepl
```

On macOS, the build produces `build/skrepl.app` (or `build/repl_demo`), bundles universal dylibs, and applies ad-hoc signatures with `codesign`.

### Build Targets Summary

- `replfltk`: Static library target (`libreplfltk.a`).
- `repl_demo`: Standalone generic C REPL example application (`examples/demo/main.c`).
- `skred_repl`: Skred audio synthesizer REPL application executable (`skrepl`).

### CMake Build Options

- `-DSKRED_BUILD_REPL=OFF`: Disable Skred download/build to compile only the generic `replfltk` library and `repl_demo`.
- `-DREPL_FLTK_DIR=/path/to/fltk`: Use a local offline FLTK 1.4 source directory instead of downloading FLTK.
- `-DSKRED_VERSION=0.54.4`: Select a specific Skred release version.

---

## Verification & CTest Suite

Run the automated test suite covering SVG rendering, spectrogram log/linear scaling, waveform zero-crossing triggers, voice topology diagrams, and vector fonts:

```sh
ctest --test-dir build --output-on-failure
```

Verify linked release info without opening a window:

```sh
./build/skrepl --check
```

---

## REPL Commands Reference

The following GUI commands are built in:

```text
help            - list registered commands
clear           - clear scrollback text
theme light|dark - switch interface color themes
font            - open GUI font chooser dialog or set font [font ["name" [size]]]
edit [file]     - open built-in script text editor (Ctrl+Enter to evaluate)
udp             - attach/send via UDP [connect <host> <port> | send <text> | mode forward|log|off | color <1..255> | status | disconnect]
bitmap          - control default graphic output window [show|hide|clear]
spectrogram     - render spectrogram [wave <slot> | record [-1|0|1] | scale [linear|log] | cmap [magma|viridis|crt|amber|gray]]
waveform        - render oscilloscope waveform [wave <slot> | record [-1|0|1] | trigger [on|off]]
topology        - render voice topology diagram [voice] [depth]
panel           - manage control panels [load|list|get|set|dump|reload|show|hide]
quit / exit     - close REPL session
```

Input that does not match a registered GUI command is passed directly to the host language line handler registered via `repl_set_fallback_handler()`.

---

## Project Structure & Module Organization

- `include/repl/`: Public C-compatible header files.
  - `repl_api.h`: Core REPL lifecycle, line callbacks, themes, fonts, dialogs.
  - `foreign_bridge.h`: Thread-safe foreign language bridge (`foreign_bridge_dispatch`).
  - `panel_dsl.h`: Interactive widget panel DSL parser and runtime.
  - `bitmap_win.h`: Bitmap image and audio metric plot windows.
  - `repl_prefs.h`: Persistent configuration preferences.
- `src/`: Core C++ implementation (`TerminalView`, `Theme`, `Spectrogram`, `Waveform`, `VoiceTopology`, `AudioMetrics`, `VectorFont`).
- `examples/`:
  - `examples/demo/`: Standalone generic C REPL application (`repl_demo`).
  - `examples/skred/`: Skred synthesizer REPL application (`skrepl`).
- `docs/`:
  - `docs/INTEGRATION.md`: Integration & language binding guide for Elixir, Forth, Python, and C/C++.
- `tests/`: Headless CTest verification suite.
- `third_party/pikchr/`: Vendored Pikchr SVG diagram generator source.

---

## License & Credits

- [FLTK](https://www.fltk.org/) 1.4.5 provides cross-platform GUI and SVG rendering.
- [miniaudio](https://miniaud.io/) 0.11.25 provides audio I/O through linked Skred release libraries.
- [Pikchr](https://pikchr.org/) 1.0.0 renders voice topology diagrams.
- [Octetta](https://github.com/octetta) - Creator and maintainer.
