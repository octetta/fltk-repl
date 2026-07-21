# Skred FLTK REPL

A desktop front end for Skred built around a reusable FLTK terminal widget.
The application links to the packaged Skred/PULP release API rather than a
PULP build tree. Input that is not a GUI command is forwarded as a complete,
unchanged line to `skred_command()`.

The terminal supports scrollback, command history, selection and clipboard
operations, light and dark themes, configurable fonts, and native FLTK
behavior on Linux, macOS, and Windows. The library also includes a bitmap
output window and a small DSL for building Skred control panels.

## Build

The default build requires CMake 3.16 or newer, a C/C++ toolchain, `curl`,
`tar`, and network access on the first configure:

```sh
cmake -S . -B build
cmake --build build -j
./build/skred_repl
```

The Makefile provides shortcuts for the same workflow:

```sh
make          # configure a Release build and compile it
make run      # build and launch the application
make info     # show relevant cached CMake settings
make clean    # remove build/
```

CMake downloads the selected maxed Skred release from the PULP GitHub
releases into `vendor/`. Version 0.51.0 is the default; select another
published version at configure time with:

```sh
cmake -S . -B build -DSKRED_VERSION=0.51.0
```

The current CMake implementation derives the package location from
`SKRED_VERSION`; `SKRED_ROOT` is not a supported override. For an offline
build, pre-populate `vendor/skred-<version>-maxed/` with `include/skred/api.h`
and `lib64/libapi` or `lib/libapi` before configuring.

FLTK 1.3.9 is fetched separately through CMake's `FetchContent`. To use a
local FLTK source tree instead:

```sh
cmake -S . -B build -DREPL_FLTK_DIR=/path/to/fltk
```

Alternatively, place the source at `third_party/fltk/`. Set
`-DREPL_FETCH_FLTK=OFF` to prevent network fallback. See
[third_party/README.md](third_party/README.md).

Use `-DSKRED_BUILD_REPL=OFF` to build only the reusable `replfltk` static
library. `examples/demo/main.c` is retained as an API example but is not
currently registered as a CMake target.

## Verify the linked release

Print the linked Skred version and feature list without creating a window or
starting audio:

```sh
./build/skred_repl --check
```

## Command-line options

```text
-v, --voices N       voice count, 1..64 (default 32)
-r, --frames N       positive audio frames per callback (default 128)
-p, --port N         UDP control port (default 0, disabled)
-o, --output N       playback device index (default -1)
-i, --input N        capture device index (default -1; -2 disables capture)
    --check           print release information and exit
-h, --help            show usage and exit
```

Compact short forms such as `-v32`, `-r128`, `-p60440`, `-o0`, and `-i-2`
are accepted.

## REPL commands

The prompt is `# `. These commands are handled by the GUI:

```text
help
clear
theme light|dark
font ["font name" [size]]
bitmap [show|hide|clear]
panel load <file.pnl>
panel reload
panel hide
boot [voices N] [frames N] [port N]
quit
exit
```

`font` with no arguments reports the current font and lists detected
monospace fonts. `boot` stops and restarts the Skred engine, retaining current
values for settings not supplied. Panel widget commands and all other input
are passed to Skred.

Use Up/Down for history. Home or Ctrl/Cmd+A moves to the start of live input;
End or Ctrl/Cmd+E moves to its end. Ctrl/Cmd+U clears back to the prompt,
Ctrl/Cmd+K clears forward, Ctrl/Cmd+W deletes the preceding word, and Escape
clears the line. Ctrl/Cmd with `+`, `-`, or `0` adjusts or resets font size.
A right-click menu provides Copy, Paste, and Clear Line.

## Bitmap and panel windows

`bitmap` controls the default graphics window. Image producers use the public
`bitmap_win_set_rgb()` or `bitmap_win_set_gray()` C APIs; until an image is
provided, showing the window displays an empty canvas. The current registry
uses one shared bitmap window even when callers request different names.

Load the included panel example from the repository root with:

```text
panel load controls.pnl
```

The panel DSL supports sliders, numeric fields, toggles, buttons, choices,
labels, and weighted rows. Widget templates substitute `%d`, `%f`, or `%s`
and send the resulting command to Skred. `panel reload` reparses the last
loaded path while preserving the panel window. Parse failures are reported to
stderr. See [`include/repl/panel_dsl.h`](include/repl/panel_dsl.h) for the DSL
grammar and [`include/repl/bitmap_win.h`](include/repl/bitmap_win.h) for the
bitmap API.

All bitmap and panel API calls must run on the FLTK main thread.

## Integration overview

The public C API is declared under `include/repl/`; implementation files are
under `src/`. `repl_set_fallback_handler()` lets an embedding application keep
a small set of registered GUI commands while routing every other original
line to its own language runtime. `examples/skred/main.c` demonstrates this
pattern, copies input into the mutable buffer required by `skred_command()`,
prints `skred_log()`, and shuts down the Skred dispatcher and audio engine
when the application exits.
