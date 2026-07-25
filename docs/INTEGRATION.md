# `fltk-repl` Integration & Foreign Language Binding Guide

`fltk-repl` is a lightweight, embeddable, cross-platform C99/C++17 desktop GUI REPL terminal component built on FLTK 1.4+. While originally created as the desktop REPL for the Skred synthesizer, `fltk-repl` is designed from the ground up to serve as a universal desktop frontend for **any** programming language, runtime, or domain-specific engine.

Whether you are building an interactive environment for **Elixir**, **Forth**, **Python**, **Lisp**, **Lua**, **Scheme**, or **C/C++**, `fltk-repl` provides a complete, thread-safe desktop terminal with:

- **ANSI 256-Color SGR Rendering**: Full support for terminal color escape sequences (`\033[38;5;Nm`).
- **Unicode Braille Graphics**: Native 2x4 dot pattern (`U+2800..U+28FF`) rendering for inline sparklines, waveforms, and visual plots.
- **Dynamic Parameter Panels (`panel_dsl`)**: Interactive GUI control panels with sliders, knobs, toggles, sub-panels, and button grids.
- **Standalone Bitmap & Audio Display Windows (`bitmap_win`)**: High-performance windows for RGB pixel buffers, audio waveforms, and spectrograms.
- **Thread-Safe Foreign Bridge (`foreign_bridge`)**: Safe line dispatching across thread boundaries (e.g. from Erlang/BEAM async tasks, Forth threads, or background audio loops).
- **Pure C99 API**: Clean, non-mangled C headers (`include/repl/*.h`) — no C++ compilation required for host applications.

---

## Table of Contents

1. [Architecture & Threading Model](#1-architecture--threading-model)
2. [C API Overview](#2-c-api-overview)
3. [Elixir / Erlang (BEAM) Integration Guide](#3-elixir--erlang-beam-integration-guide)
4. [Forth Integration Guide](#4-forth-integration-guide)
5. [Python Integration Guide](#5-python-integration-guide)
6. [Embedded C / C++ Integration Guide](#6-embedded-c--c-integration-guide)
7. [GUI Control Panels (`panel_dsl`)](#7-gui-control-panels-panel_dsl)
8. [Bitmap & Audio Visualizations (`bitmap_win`)](#8-bitmap--audio-visualizations-bitmap_win)

---

## 1. Architecture & Threading Model

### Public C Headers (`include/repl/`)

The public interface consists of five pure C99 header files under `include/repl/`:

- [`repl_api.h`](file:///home/stewartj/book/fltk-repl/include/repl/repl_api.h): Core REPL creation, prompt management, line dispatching, command registration, themes, fonts, and file dialogs.
- [`foreign_bridge.h`](file:///home/stewartj/book/fltk-repl/include/repl/foreign_bridge.h): Thread-safe marshalling for foreign runtimes (Erlang/Elixir, Forth, Python) that dispatch lines from background threads onto the FLTK main loop via `Fl::awake()`.
- [`panel_dsl.h`](file:///home/stewartj/book/fltk-repl/include/repl/panel_dsl.h): C API for parsing DSL layout files and controlling interactive widget panels (sliders, knobs, toggles, choices).
- [`bitmap_win.h`](file:///home/stewartj/book/fltk-repl/include/repl/bitmap_win.h): C API for creating standalone graphic windows, RGB buffers, oscilloscope waveforms, and audio spectrograms.
- [`repl_prefs.h`](file:///home/stewartj/book/fltk-repl/include/repl/repl_prefs.h): Cross-platform preference storage for window state, themes, and settings.

### Threading Rules

1. **FLTK Main Thread**: GUI creation (`repl_create`), rendering, and FLTK event loop execution (`repl_run`) must happen on the main OS thread.
2. **Foreign Background Threads**: If your host language executes commands on a worker thread (e.g. Erlang BEAM scheduler process, Gforth task thread), use `foreign_bridge_dispatch(line)` to send input lines safely across thread boundaries.

---

## 2. C API Overview

### Minimal C Host Application

```c
#include <repl/repl_api.h>
#include <stdio.h>

// Fallback line handler for un-registered commands
static void on_line_entered(const char *line, void *userdata) {
    repl_ctx *ctx = (repl_ctx *)userdata;
    repl_printf(ctx, "Evaluated line: %s\n", line);
}

int main(void) {
    // 1. Create REPL window (title, width, height)
    repl_ctx *ctx = repl_create("My Custom REPL", 900, 600);
    
    // 2. Register standard built-in commands (help, clear, theme, font, quit)
    repl_register_default_commands(ctx);

    // 3. Register custom line handler for language evaluation
    repl_set_fallback_handler(ctx, on_line_entered, ctx);

    // 4. Set prompt string
    repl_set_prompt(ctx, "in> ");

    // 5. Welcome message
    repl_println(ctx, "\x1b[38;5;51mWelcome to My Custom Language REPL!\x1b[0m");

    // 6. Enter event loop (blocks until window is closed)
    int rc = repl_run(ctx);

    // 7. Cleanup
    repl_destroy(ctx);
    return rc;
}
```

---

## 3. Elixir / Erlang (BEAM) Integration Guide

Elixir and Erlang applications can easily embed `fltk-repl` as an interactive desktop REPL using either a **C Port Driver** (external process communicating over stdin/stdout or pipe) or an **Erlang NIF** (Native Implemented Function).

### Approach: Elixir Port Process / C Bridge

1. **C Bridge (`elixir_repl_bridge.c`)**:
   - Initializes `repl_create()`.
   - Listens for lines entered in the GUI and sends them as JSON or raw lines over standard output (or UNIX socket) to the BEAM process.
   - Listens for evaluation results from BEAM over standard input (or socket) and calls `repl_print()`.

```c
#include <repl/repl_api.h>
#include <repl/foreign_bridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static repl_ctx *g_repl = NULL;

// Callback when user hits Enter in the GUI
static void on_elixir_line(const char *line, void *ud) {
    (void)ud;
    // Send input line to Elixir BEAM process via stdout
    printf("EVAL:%s\n", line);
    fflush(stdout);
}

int main(void) {
    g_repl = repl_create("Elixir Desktop REPL", 950, 650);
    repl_register_default_commands(g_repl);
    repl_set_fallback_handler(g_repl, on_elixir_line, NULL);
    repl_set_prompt(g_repl, "iex> ");

    repl_println(g_repl, "Interactive Elixir (BEAM) Desktop Environment");

    return repl_run(g_repl);
}
```

2. **Elixir Host Code (`lib/elixir_repl.ex`)**:

```elixir
defmodule ElixirRepl do
  @doc """
  Spawns the fltk-repl C desktop process and evaluates incoming Elixir code.
  """
  def start_link do
    port = Port.open({:spawn, "./build/elixir_repl_bridge"}, [:line, :use_stdio])
    loop(port, Code.binding())
  end

  defp loop(port, binding) do
    receive do
      {^port, {:data, {:line, 'EVAL:' ++ line_chars}}} ->
        line = to_string(line_chars)
        {result, new_binding} = try_eval(line, binding)
        # Format result and send back to C REPL
        send_to_repl(port, result)
        loop(port, new_binding)

      {^port, {:exit_status, status}} ->
        IO.puts("REPL exited with status #{status}")
    end
  end

  defp try_eval(line, binding) do
    try do
      {result, new_binding} = Code.eval_string(line, binding)
      {inspect(result), new_binding}
    rescue
      e -> {"** (Error) " <> Exception.message(e), binding}
    end
  end

  defp send_to_repl(port, output) do
    Port.command(port, output <> "\n")
  end
end
```

---

## 4. Forth Integration Guide

Forth systems (such as **Gforth**, **VFX Forth**, or custom C-based Forth runtimes) excel at dynamic interactive development. `fltk-repl` provides a seamless desktop console for Forth using standard C FFI declarations (`c-function`).

### Gforth C FFI Binding (`fltk_repl.fs`)

```forth
\ fltk_repl.fs - Gforth bindings for fltk-repl

c-library repl_lib
  s" replfltk" add-lib
  s" fltk" add-lib
  s" stdc++" add-lib

  \ C API Declarations
  c-function repl_create repl_create a i i -- a
  c-function repl_destroy repl_destroy a -- void
  c-function repl_run repl_run a -- i
  c-function repl_set_prompt repl_set_prompt a a -- void
  c-function repl_println repl_println a a -- void
  c-function repl_set_fallback_handler repl_set_fallback_handler a a a -- void
end-c-library

\ Forth Line Evaluator Callback
:noname ( line_ptr user_data -- )
  drop z>s evaluate
; 2cb: forth-eval-cb

: start-forth-repl ( -- )
  s" Forth FLTK Desktop Console" drop 900 600 repl_create { ctx }
  ctx s" forth> " drop repl_set_prompt
  ctx s" \x1b[38;5;226mGforth GUI Terminal Active\x1b[0m" drop repl_println
  ctx forth-eval-cb 0 repl_set_fallback_handler
  ctx repl_run drop
  ctx repl_destroy
;

start-forth-repl
```

---

## 5. Python Integration Guide

Python can embed `fltk-repl` directly via `ctypes` or `cffi`, providing a rich desktop GUI shell for Python code execution.

### Python Script (`python_repl.py`)

```python
import ctypes
import sys

# Load libreplfltk shared library
repl_lib = ctypes.CDLL("./build/libreplfltk.so")

# Configure C types
repl_lib.repl_create.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
repl_lib.repl_create.restype = ctypes.c_void_p

repl_lib.repl_set_prompt.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
repl_lib.repl_println.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
repl_lib.repl_run.argtypes = [ctypes.c_void_p]
repl_lib.repl_run.restype = ctypes.c_int

# Define Line Callback Function Type
LINE_FN = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_void_p)

# Global execution context
globals_dict = {}

def python_eval_callback(line_bytes, userdata):
    line = line_bytes.decode('utf-8')
    try:
        # Try evaluating expression
        result = eval(line, globals_dict)
        if result is not None:
            repl_lib.repl_println(ctx, str(result).encode('utf-8'))
    except SyntaxError:
        # If syntax error, try executing statement
        try:
            exec(line, globals_dict)
        except Exception as e:
            repl_lib.repl_println(ctx, f"Error: {e}".encode('utf-8'))
    except Exception as e:
        repl_lib.repl_println(ctx, f"Error: {e}".encode('utf-8'))

cb_func = LINE_FN(python_eval_callback)

# Create REPL Session
ctx = repl_lib.repl_create(b"Python FLTK Desktop REPL", 900, 600)
repl_lib.repl_set_prompt(ctx, b">>> ")
repl_lib.repl_println(ctx, b"\x1b[38;5;208mPython 3 Desktop REPL Shell\x1b[0m")
repl_lib.repl_set_fallback_handler(ctx, cb_func, None)

# Run Event Loop
sys.exit(repl_lib.repl_run(ctx))
```

---

## 6. Embedded C / C++ Integration Guide

For C or C++ interpreters (such as LUA, Scheme, Tcl, or custom DSLs), embed `fltk-repl` by linking `libreplfltk.a` directly into your application executable.

```c
#include <repl/repl_api.h>
#include <stdio.h>
#include <stdlib.h>

static void my_interpreter_eval(const char *line, void *userdata) {
    repl_ctx *ctx = (repl_ctx *)userdata;
    // Call your language engine parser/evaluator here
    repl_printf(ctx, "==> %s\n", line);
}

int main(int argc, char **argv) {
    repl_ctx *ctx = repl_create("Embedded C REPL", 900, 600);
    repl_register_default_commands(ctx);
    repl_set_fallback_handler(ctx, my_interpreter_eval, ctx);
    repl_set_prompt(ctx, "eval> ");
    
    return repl_run(ctx);
}
```

---

## 7. CLI Subprocess Pipe Wrapper Pattern (`repl_add_fd`)

`fltk-repl` can serve as a desktop GUI frontend for **external, line-buffered command-line programs** (such as `sqlite3`, `bc`, `python -i`, `gforth`, or custom CLI binaries).

Using `repl_add_fd()`, FLTK's main event loop monitors the child process's `stdout` pipe and prints output asynchronously without requiring background threads or polling.

### Subprocess Pipe Wrapper Architecture (`examples/pipe_demo/main.c`)

```c
#include <repl/repl_api.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct {
    repl_ctx *ctx;
    int stdin_fd;
    int stdout_fd;
} child_wrapper_t;

// 1. User types in GUI -> forward to child process stdin pipe
static void on_line_entered(const char *line, void *userdata) {
    child_wrapper_t *w = (child_wrapper_t *)userdata;
    write(w->stdin_fd, line, strlen(line));
    write(w->stdin_fd, "\n", 1);
}

// 2. Child process prints to stdout -> FLTK event loop triggers callback
static void on_child_stdout_ready(int fd, void *userdata) {
    child_wrapper_t *w = (child_wrapper_t *)userdata;
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        repl_print(w->ctx, buf); // Display in REPL scrollback
    } else if (n == 0) {
        repl_remove_fd(fd);
        repl_println(w->ctx, "\n[Subprocess exited]");
    }
}

int main(int argc, char **argv) {
    repl_ctx *ctx = repl_create("CLI Subprocess Wrapper", 920, 600);
    child_wrapper_t w = { .ctx = ctx };

    // Spawn child process (e.g. "bc -l" or "sqlite3") with pipes...
    spawn_child_pipes(&w, "bc -l");

    repl_set_fallback_handler(ctx, on_line_entered, &w);
    
    // Register child stdout file descriptor with FLTK event loop
    repl_add_fd(w.stdout_fd, on_child_stdout_ready, &w);

    return repl_run(ctx);
}
```

Try the included CLI wrapper demo:

```sh
# Run arbitrary CLI program inside fltk-repl
./build/repl_pipe_demo "bc -l"
./build/repl_pipe_demo "sqlite3"
```

---

## 8. GUI Control Panels (`panel_dsl`)

Host applications can launch interactive parameter control panels by loading a `.pnl` DSL file or dynamically invoking panel API calls:

```text
panel load synth demo.pnl cutoff=440 resonance=0.7
panel set synth cutoff 880 1
panel dump synth
```

### DSL Example (`demo.pnl`)

```text
panel "Synth Voice Controller" {
    row {
        slider "Cutoff (Hz)" cutoff 20 20000 440
        slider "Resonance" res 0.0 1.0 0.7
    }
    row {
        toggle "Filter Active" filt_on 1
        choice "Waveform" osc_type "Sine" "Saw" "Square" "Triangle"
    }
}
```

When widgets move, they trigger callbacks back to the host application via `panel_dsl.h`.

---

## 8. Bitmap & Audio Visualizations (`bitmap_win`)

Applications can render real-time oscilloscope waveforms, audio spectrograms, and pixel graphics in separate dedicated FLTK windows:

```c
#include <repl/bitmap_win.h>

// Open a standalone bitmap visualization window
bitmap_win_t *bw = bitmap_win_get("wave_display");
bitmap_win_show(bw);

// Render audio waveform (interleaved float samples)
bitmap_win_set_waveform(bw, "Oscilloscope Output", samples, n_frames, n_channels);

// Render spectrogram
bitmap_win_set_spectrogram_labeled(bw, "Spectral Density", samples, n_frames, n_channels);
```

---

## Build Targets & CMake Options

```sh
# Configure Release Build
cmake -S . -B build

# Build Standalone Generic REPL Demo (examples/demo/main.c)
cmake --build build --target repl_demo -j

# Run Generic Demo
./build/repl_demo

# Build Skred Synthesizer REPL Application (skrepl)
cmake --build build --target skred_repl -j

# Run Test Suite
ctest --test-dir build --output-on-failure
```
