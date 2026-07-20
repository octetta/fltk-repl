# fltk-repl

A small, cross-platform (Linux/macOS/Windows) REPL / shell-style
widget built on [FLTK](https://www.fltk.org), exposed as a **plain C
API** so you can build a text-in/text-out console for your own tool
without needing to know C++ or FLTK internals.

Point of the project: you have some C/C++ function you want a
terminal-style front end for -- an audio engine, a parser, a build
tool, whatever -- and you want:

- a real scrollback shell (not a text box + separate log pane): output
  appears right after the command that produced it, exactly like a
  terminal
- Up/Down arrow history recall
- select-and-copy from anywhere in the window, paste back in
- light/dark theme + configurable colors and font
- native Open/Save/Choose-folder file dialogs
- full Unicode output, including Braille pattern characters
  (U+2800-U+28FF) for terminal-style waveform/spectrogram display
- a build that "just works": `cmake -B build && cmake --build build`
  on a fresh `git clone`, no submodules, no pre-installed FLTK

## Quick start

```sh
git clone <this-repo>
cd fltk-repl
cmake -B build
cmake --build build -j
./build/repl_demo          # Linux/macOS
# build\Debug\repl_demo.exe  on Windows with a multi-config generator
```

The first `cmake -B build` configure step downloads FLTK's source via
`FetchContent` and builds it as part of this project -- no manual
`git submodule` step, no system package required. See
[third_party/README.md](third_party/README.md) if you want an
offline/pinned build instead.

Try typing `help`, `wave 5`, `noise`, `open`, `theme light`, `font
"DejaVu Sans Mono" 16` in the demo window.

## Using it in your own project

You write plain C against `include/repl/repl_api.h`. Nothing in that
header requires C++ knowledge -- it's an opaque handle plus function
pointers.

```c
#include "repl/repl_api.h"

static void cmd_hello(int argc, char **argv, void *userdata) {
    repl_ctx *r = (repl_ctx *)userdata;
    repl_println(r, "hi there!");
}

int main(void) {
    repl_ctx *r = repl_create("My Tool", 900, 600);
    repl_register_default_commands(r);   /* help, clear, theme, font, quit */
    repl_register_command(r, "hello", cmd_hello, r);
    repl_println(r, "Type 'help' to list commands.");

    int rc = repl_run(r);
    repl_destroy(r);
    return rc;
}
```

Link your target against the `replfltk` CMake target:

```cmake
add_subdirectory(fltk-repl)
add_executable(my_tool main.c)
target_link_libraries(my_tool PRIVATE replfltk)
set_target_properties(my_tool PROPERTIES LINKER_LANGUAGE CXX)
```

The `LINKER_LANGUAGE CXX` line matters even for a pure-`.c` source
file: FLTK itself is C++, so the final link step needs the C++
runtime. CMake figures this out for you if you tell it the linker
language explicitly (the demo target does the same thing -- see the
top-level `CMakeLists.txt`).

See `examples/demo/main.c` for a complete, runnable example including
the file dialogs and the Braille waveform renderer.

## API overview

Full documented header: [`include/repl/repl_api.h`](include/repl/repl_api.h).

| Concern | Functions |
|---|---|
| lifecycle | `repl_create`, `repl_destroy`, `repl_run`, `repl_quit` |
| commands | `repl_register_command`, `repl_unregister_command`, `repl_register_default_commands` |
| output | `repl_print`, `repl_println`, `repl_printf`, `repl_clear`, `repl_set_prompt` |
| history | `repl_history_count`, `repl_history_get`, `repl_history_clear` |
| theming | `repl_set_theme`, `repl_get_theme`, `repl_set_colors`, `repl_set_font`, `repl_set_font_size`, `repl_list_fonts` |
| dialogs | `repl_open_file_dialog`, `repl_save_file_dialog`, `repl_choose_directory_dialog`, `repl_free_string` |

### Command callbacks

```c
typedef void (*repl_cmd_fn)(int argc, char **argv, void *userdata);
```

Just like `main()`'s argv: `argv[0]` is the command name, the rest are
whitespace-separated arguments. Quoting works (`load "my file.wav"`
gives you `argv[1] == "my file.wav"`).

### Output

Call `repl_print`/`repl_println`/`repl_printf` from inside a command
callback (or any time after `repl_create`) to append to the
scrollback. The next prompt is shown automatically after your command
callback returns -- you don't manage that yourself.

### Input handling / keybindings

The whole scrollback + input line lives in one real, selectable text
widget, so copy/paste from anywhere in the window works like a normal
terminal:

- **Enter**: submit the current input line
- **Up / Down**: recall older/newer history (only within the input
  line; scrollback above it is untouched)
- **Home**: jump to the start of the current input line (not the
  start of the whole buffer)
- **Backspace / typing while the cursor is in old scrollback**: the
  widget snaps you back to the live input area rather than letting
  you edit already-printed output
- normal mouse click-drag selection and Ctrl+C / Ctrl+V (Cmd on
  macOS) work everywhere, including across old output

### Theming

```c
repl_set_theme(r, REPL_THEME_DARK);   /* or REPL_THEME_LIGHT */
repl_set_colors(r, 0x1e1e1e, 0xd4d4d4, 0x4ec9b0, 0xffffff); /* bg, fg, prompt, input-echo */
repl_set_font(r, "DejaVu Sans Mono", 15);
```

`repl_register_default_commands` also wires up `theme light|dark` and
`font <name> <size>` as things the *user* can type at runtime, so you
often don't need to build your own settings UI at all.

#### A note on the Braille/Unicode font

Braille pattern characters (U+2800-U+28FF) are used by the demo's
`wave`/`noise` commands to render a 2x4-dots-per-character
sparkline/spectrogram, the same trick many terminal audio tools use
for compact waveform display. Rendering quality depends entirely on
the font you pick:

- **DejaVu Sans Mono** and **Noto Sans Mono** both have full Braille
  coverage and are commonly present on Linux; DejaVu ships with most
  distros already.
- **Menlo** / **SF Mono** on macOS render Braille fine.
- On Windows, **Consolas** does *not* reliably cover the Braille
  block on all versions -- **Cascadia Mono** or **DejaVu Sans Mono**
  (installed alongside your app, or via `repl_set_font` pointing at a
  font you ship) are safer bets.

Use `repl_list_fonts()` to enumerate what's actually installed and
build a picker, or just call `font <name> <size>` from the built-in
command to try one interactively.

### File dialogs

```c
char *path = repl_open_file_dialog(r, "Open a file", "WAV Files\t*.wav\nAll Files\t*");
if (path) {
    /* use path */
    repl_free_string(path);
}
```

These wrap FLTK's native file chooser (`Fl_Native_File_Chooser`), so
you get the real OS dialog on each platform, not an FLTK-drawn
imitation.

## Vendoring / offline builds

This repo never uses git submodules. FLTK is acquired one of three
ways, in this order (see `cmake/FetchFLTK.cmake`):

1. `-DREPL_FLTK_DIR=/path/to/fltk-source` -- point at any local FLTK
   checkout.
2. `third_party/fltk/` -- if you've copied (or `git clone`d,
   independently of this repo's own git history) an FLTK source tree
   there, it's used automatically. See
   [`third_party/README.md`](third_party/README.md).
3. `FetchContent` from `https://github.com/fltk/fltk.git` -- the
   default, happens automatically the first time you configure with
   network access.

To pin a different FLTK version, pass
`-DREPL_FLTK_GIT_TAG=release-1.4.0` (or any tag/branch) at configure
time, or edit the default in `cmake/FetchFLTK.cmake`.

For a fully offline build with none of the above prepared yet, add
`-DREPL_FETCH_FLTK=OFF` and CMake will fail fast with instructions
instead of quietly trying to reach the network.

## Project layout

```
include/repl/repl_api.h   the public C API -- this is the only header you need
src/repl_api.cpp           C API implementation (glue between C API and TerminalView)
src/TerminalView.{h,cpp}   the shell-like scrollback+input FLTK widget (C++)
src/Theme.{h,cpp}          light/dark palette + FLTK global scheme handling
src/Tokenize.{h,cpp}       argv-style command-line tokenizer (quoting support)
examples/demo/main.c       runnable example, plain C, includes Braille plot demo
cmake/FetchFLTK.cmake      FLTK acquisition logic (FetchContent / local / vendored)
third_party/               empty by default; optional manual FLTK vendoring lives here
```

If you never plan to touch C++, the only files you should ever need
to open are `include/repl/repl_api.h` and `examples/demo/main.c`.

## Known limitations

- Selecting text that spans from old scrollback into the live input
  line, then typing over it, is not specially guarded beyond snapping
  the cursor to the end of input -- the selection itself gets cleared
  first, which is safe but means the old-output half of the selection
  isn't inserted anywhere. This matches how most real terminals behave
  in that situation.
- The style/coloring model is intentionally simple (three styles:
  output text, prompt, input echo) rather than full ANSI escape code
  parsing. If you need `\x1b[31m`-style color codes from your API's
  text output, strip/translate them to `repl_set_colors` calls or
  extend `TerminalView`'s style table (it's a small, well-commented
  class).
