# Skred FLTK REPL

A desktop REPL for Skred, built from the reusable FLTK terminal in this
directory and linked to a **packaged PULP release API**. It does not link
against PULP's build tree.

The terminal provides scrollback, selection/copy/paste, command history,
light and dark themes, configurable fonts, and native FLTK behavior on
Linux, macOS, and Windows. Skred commands are forwarded as complete,
unchanged lines to `skred_command()`.

## Build

With a PULP checkout at `../../../pulp`, CMake automatically selects the
newest matching maxed release for the current platform:

```sh
cmake -S . -B build
cmake --build build -j
./build/skred_repl
```

The same commands are available as:

```sh
make
make run
```

To select a specific installed or unpacked release:

```sh
cmake -S . -B build \
  -DSKRED_ROOT=/path/to/skred-0.51.0-maxed
cmake --build build -j
```

`SKRED_ROOT` must contain `include/skred/api.h` and `lib64/libapi` (or
`lib/libapi`). It may also be supplied as an environment variable. The
configured release library directory is placed in the executable's build
RPATH, so `build/skred_repl` can be run directly.

The first FLTK configure may download FLTK. For offline builds, either set
`REPL_FLTK_DIR` to an FLTK source tree or place one in `third_party/fltk`.
See [third_party/README.md](third_party/README.md).

To verify the selected release without opening a window or starting audio:

```sh
./build/skred_repl --check
```

## Running

Defaults follow `mini-skred` where useful, except UDP is disabled unless
requested:

```text
-v, --voices N       voice count (default 32)
-r, --frames N       requested audio frames (default 128)
-p, --port N         UDP control port (default 0)
-o, --output N       playback device index (default -1)
-i, --input N        capture device index (-2 disables capture)
```

Compact forms such as `-v32`, `-r128`, and `-p60440` are also accepted.

Inside the window, every line not recognized as a GUI command goes directly
to Skred. The GUI-only commands are:

```text
help
clear
theme light|dark
font "font name" [size]
quit
exit
```

Use Up/Down for history. Normal mouse selection, copy, and paste work across
the scrollback and current input line.

## Integration details

The generic FLTK C API now has `repl_set_fallback_handler()`. Registered GUI
commands are dispatched normally; any other input invokes the fallback with
the original line. `examples/skred/main.c` uses that callback to:

1. make the mutable copy required by `skred_command()`;
2. print `skred_log()` into the terminal;
3. preserve `mini-skred`'s positive return display and negative-return exit;
4. stop Skred's control dispatcher and audio engine on application exit.

The generic FLTK demo remains available with
`-DREPL_BUILD_DEMO=ON`; the Skred application can be disabled with
`-DSKRED_BUILD_REPL=OFF`.
