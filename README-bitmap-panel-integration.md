# Adding `bitmap_win` and `panel_dsl` to the Skred REPL

Two optional GUI extensions for `fltk-repl`:

- **`bitmap_win`** — one graphics-output window that any Skred command can
  draw into (spectrograms, scope snapshots, wavetable previews, ...).
- **`panel_dsl`** — declarative control panels (sliders/fields/toggles/
  buttons/choices) loaded from a small text file, each control firing a
  Skred command.

Both follow the same shape as the rest of the REPL's C API: plain C
headers, C++ implementations against FLTK, no new third-party
dependencies. This doc assumes the layout implied by the repo's own
README (`include/repl/`, `src/`, `examples/skred/main.c`,
`repl_set_fallback_handler()`, `skred_command()`, `skred_log()`) — check
your actual file/function names against this before pasting, since I'm
working from that description rather than the live source tree.

---

## 1. Add the files

```
include/repl/bitmap_win.h
src/bitmap_win.cpp

include/repl/panel_dsl.h
src/panel_dsl.cpp
```

Both `.cpp` files only need FLTK — no other third-party libs, no changes
to `third_party/`.

---

## 2. CMakeLists.txt

Add both `.cpp` files to whatever target already builds the REPL/Skred
sources (the same target that compiles your existing `repl_*.cpp` files):

```cmake
target_sources(skred_repl PRIVATE
    # ...existing sources...
    src/bitmap_win.cpp
    src/panel_dsl.cpp
)
```

If your CMake uses a glob (`file(GLOB ...)`) instead of an explicit list,
no change is needed — just make sure `src/*.cpp` covers the new files.

No new `find_package`/`target_link_libraries` entries are required;
both files link against whatever FLTK target you already use.

---

## 3. Wire the command dispatch (`examples/skred/main.c`)

Both libraries call back into Skred through a single function pointer
each command's widget fires. Register these once, near wherever
`repl_set_fallback_handler()` is already called:

```c
#include "repl/bitmap_win.h"
#include "repl/panel_dsl.h"

/* panel_dsl calls this whenever a panel widget fires. Route it straight
 * into the same command path the text REPL uses. */
static void panel_to_skred(const char *line, void *user_data) {
    (void)user_data;
    skred_command(line);
}

int main(int argc, char **argv) {
    // ...existing setup...

    repl_set_fallback_handler(my_fallback_handler);
    panel_set_command_handler(panel_to_skred, NULL);

    // ...Fl::run() etc...
}
```

`bitmap_win` doesn't need a registration call — commands just fetch a
window handle on demand (see below).

---

## 4. Add GUI-only REPL commands

Inside your existing fallback handler (the same place `theme`/`font`
GUI-only commands are handled), add cases for the bitmap window and
panel loading/reloading:

```c
int my_fallback_handler(const char *line) {
    char cmd[64], arg[256];
    int n = sscanf(line, "%63s %255[^\n]", cmd, arg);

    if (strcmp(cmd, "bitmap") == 0) {
        bitmap_win_t *bw = bitmap_win_get("default");
        if (n >= 2 && strcmp(arg, "show") == 0) { bitmap_win_show(bw); return 1; }
        if (n >= 2 && strcmp(arg, "hide") == 0) { bitmap_win_hide(bw); return 1; }
        return 1;
    }

    if (strcmp(cmd, "panel") == 0) {
        static panel_win_t *pw = NULL;
        if (n >= 2 && strncmp(arg, "load ", 5) == 0) {
            if (pw) panel_destroy(pw);
            pw = panel_load_file(arg + 5);
            if (pw) panel_show(pw);
            return 1;
        }
        if (n >= 2 && strcmp(arg, "reload") == 0) {
            if (pw) panel_reload_file(pw, /* same path used at load */ "controls.pnl");
            return 1;
        }
        if (n >= 2 && strcmp(arg, "hide") == 0) { if (pw) panel_hide(pw); return 1; }
        return 1;
    }

    // ...existing fallback cases...
    return 0;
}
```

The `panel reload` case above hardcodes the path for brevity — in
practice, stash the path used by `panel load` in the same `static`
scope so `reload` doesn't need it repeated.

---

## 5. First `bitmap_win` producer: `wavetable_spectrogram_show`

Wherever `wavetable_spectrogram_show` currently renders to your terminal
spectrogram display, add a second output path into the bitmap window.
The exact call depends on what buffer that function already computes,
but the shape is:

```c
#include "repl/bitmap_win.h"

void wavetable_spectrogram_show(/* existing args */) {
    // ...existing terminal-spectrogram rendering...

    // NEW: also push the same magnitude data to the bitmap window
    bitmap_win_t *bw = bitmap_win_get("spectrogram");
    bitmap_win_set_title(bw, "wavetable spectrogram");
    bitmap_win_set_gray(bw, magnitude_buf, width, height); /* uint8_t*, 0-255 */
    bitmap_win_show(bw);
}
```

`bitmap_win_set_gray` expects 0–255 grayscale; if your magnitude buffer
is float dB or linear magnitude, normalize/quantize it to `uint8_t`
before calling in.

---

## 6. Example panel DSL file

Save as `controls.pnl` (or wherever `panel load` will point):

```
window "Envelope" 320 240

slider attack  0 5000 ms   "env_attack_set %d"  =1200
slider decay   0 5000 ms   "env_decay_set %d"
row
  toggle hardstop           "loop_release_hard_stop %d"  =on
  field  voices  1 32       "voice_count_set %d"         =4
endrow
choice wave  "sine saw square"   "voice_wave_set %s"  =saw
label "-- danger zone --"
button panic                "all_notes_off"
```

Full grammar and modifier docs (`=default`, `@weight`, resize behavior)
are in the comment block at the top of `panel_dsl.h`.

---

## 7. Example session

```
skred> panel load controls.pnl
skred> bitmap show
skred> wavetable_spectrogram_show some_wave.wav
```

At this point:
- The panel window is up with `attack`/`decay` sliders, the
  `hardstop`/`voices` row, the `wave` dropdown, and the `panic` button —
  each firing straight into `skred_command()`.
- The bitmap window is up and will populate the next time any command
  calls `bitmap_win_set_rgb`/`set_gray` on it (e.g. the spectrogram call
  above).
- Dragging the panel window wider stretches sliders/fields/choices to
  match; row heights stay fixed.

Editing `controls.pnl` and running `panel reload` rebuilds the panel
in place without restarting the audio engine.

---

## 8. Threading — read this before wiring in

Both libraries assume every call happens on the FLTK main thread, i.e.
from the same context `skred_command()` normally runs on (the fallback
handler dispatch path, or a widget callback). Confirm this holds for
your build:

- If Skred commands can be triggered from a non-FLTK thread (e.g. a
  scheduled/deferred event fired from the audio thread), calls to
  `bitmap_win_*` / `panel_*` from that path need to go through
  `Fl::awake(callback, data)` instead of calling directly — otherwise
  you'll get races on FLTK's internals.
- The `panel_to_skred` → `skred_command()` direction is the reverse:
  widget callbacks always run on the FLTK thread already, so that side
  is safe as long as `skred_command()` itself is safe to call from FLTK
  callbacks (it presumably already is, since the text REPL does the
  same thing).

---

## 9. Known limitations (carried over from design/testing)

- **No reverse binding.** Panel widgets show DSL-authored or default
  values, not the engine's live current value. Round-tripping "what's
  the engine's value right now" back into a slider needs either a query
  command + `skred_log()` parse, or shared-memory state like the
  `skope` IPC — deliberately deferred, not implemented.
- **Only horizontal resize tracking.** Widening/narrowing a panel window
  reflows column widths; row heights and vertical layout are fixed.
  Making rows also grow/shrink vertically is a real design decision
  (proportional scaling? extra space to the last row?) left for you to
  decide rather than guessed at.
- **`bitmap_win` keeps one window per name.** `bitmap_win_get(name)` returns
  the existing named window or creates it on first use.
- **Last column in a row absorbs rounding remainder.** Fine for 2–3
  column rows; with 5+ columns the last one may look slightly uneven
  width-wise.

---

## 10. Verification

Both libraries were compiled and exercised against real FLTK 1.3 headers
(clean under `-Wall -Wextra`) and run headlessly under Xvfb: window
creation/show, image display and letterboxing, DSL parsing (including
deliberately malformed input), file load/reload, every widget type's
callback firing with correct command substitution, and live window
resize all confirmed against actual runtime output — not just read
through. That doesn't cover your specific build environment or FLTK
version, so a local build/smoke-test after integration is still worth
doing before relying on it during a live-coding session.
