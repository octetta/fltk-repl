# Skred FLTK REPL

A desktop front end for Skred built around a reusable FLTK terminal widget.
The application links to the packaged Skred/PULP release API rather than a
PULP build tree. Input that is not a GUI command is forwarded as a complete,
unchanged line to `skred_command()`.

The skrepl version is stored in `VERSION` and printed before the linked Skred
version at startup and by `./build/skrepl --check`.

The terminal supports scrollback, command history, selection and clipboard
operations, clickable HTTP/HTTPS links, light and dark themes, configurable
fonts, and native FLTK behavior on Linux, macOS, and Windows. Links are
underlined, show a hand cursor, and open with the platform's default handler;
dragging continues to select text. The library also includes a bitmap output
window and a small DSL for building Skred control panels.

## Credits and project links

skrepl is built with these open-source projects:

- [FLTK](https://www.fltk.org/) 1.4.5 provides the cross-platform GUI and SVG rendering.
- [miniaudio](https://miniaud.io/) 0.11.25 provides audio I/O through the linked Skred 0.52.0 library.
- [Pikchr](https://pikchr.org/) 1.0.0 renders voice-topology diagrams from its vendored generated C source.

Project and Octetta pages:

- [GitHub repository](https://github.com/octetta/fltk-repl)
- [Octetta on YouTube](https://www.youtube.com/@octetta)
- [Octetta on LinkedIn](https://www.linkedin.com/in/octetta)

Startup and `--check` print the skrepl banner, the configured FLTK and Pikchr
versions, the linked miniaudio runtime version, these URLs, and the UTC build
date.

## Build

The default build requires CMake 3.16 or newer, a C/C++ toolchain, `curl`,
`tar`, and network access on the first configure:

```sh
cmake -S . -B build
cmake --build build -j
./build/skrepl
```

On macOS, the build instead produces `build/skrepl.app`, copies the
universal Skred dylibs into `Contents/Frameworks`, and applies an ad-hoc
signature with `codesign`. Disable only the signing step with
`-DFLTK_REPL_ADHOC_SIGN=OFF`; the application bundle is still produced.
`make run` selects the correct executable automatically. For a command-line
check of the bundle, run:

```sh
./build/skrepl.app/Contents/MacOS/skrepl --check
```

The Makefile provides shortcuts for the same workflow:

```sh
make          # configure a Release build and compile it
make run      # build and launch the application
make info     # show relevant cached CMake settings
make clean    # remove build/
```

CMake downloads the selected maxed Skred release from the PULP GitHub
releases into `vendor/`. Version 0.52.0 is the default; select another
published version at configure time with:

```sh
cmake -S . -B build -DSKRED_VERSION=0.52.0
```

The current CMake implementation derives the package location from
`SKRED_VERSION`; `SKRED_ROOT` is not a supported override. For an offline
build, pre-populate `vendor/skred-<version>-maxed/` with `include/skred/api.h`
and `lib64/libapi` or `lib/libapi` before configuring.

FLTK 1.4.5 is fetched separately through CMake's `FetchContent`. SVG support
is enabled and the reusable library links FLTK's image target. To use a local
FLTK source tree instead:

```sh
cmake -S . -B build -DREPL_FLTK_DIR=/path/to/fltk
```

Alternatively, place the source at `third_party/fltk/`. Set
`-DREPL_FETCH_FLTK=OFF` to prevent network fallback. See
[third_party/README.md](third_party/README.md).

FLTK 1.4.0 or newer is required. Select a different 1.4.x release with
`-DREPL_FLTK_GIT_TAG=release-1.4.x`.

Use `-DSKRED_BUILD_REPL=OFF` to build only the reusable `replfltk` static
library. `examples/demo/main.c` is retained as an API example but is not
currently registered as a CMake target.

## Verify the linked release

Print the linked Skred version and feature list without creating a window or
starting audio:

```sh
./build/skrepl --check
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
spectrogram wave <slot> | record [-1|0|1]
waveform wave <slot> | record [-1|0|1]
topology <voice> [depth]
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
`bitmap_win_set_rgb()`, `bitmap_win_set_gray()`, or
`bitmap_win_set_spectrogram()` C APIs. The spectrogram input is interleaved
floating-point audio and may select one channel or downmix all channels. Until
an image is provided, showing the window displays an empty canvas. Named
producers use separate windows, so waveform and spectrogram views can remain
visible together.

For annotated output, use `bitmap_win_set_spectrogram_labeled()` or
`bitmap_win_set_waveform()`. Their sample-rate-aware `_ex` variants accept a
rate in hertz, allowing frequency and duration labels to use physical units;
the compatibility forms pass no rate and therefore use Nyquist-normalized
frequency values. Waveform calls may also supply loop start/end frame indices,
using `-1` when a marker is unavailable. Sample buffers are copied or consumed
synchronously and remain owned by the caller.

Skred 0.52.0 supplies parser data to host foreign-function callbacks. The REPL
uses reserved slot `/ff9` to copy that data and marshal rendering onto FLTK's
main thread. Display a wavetable or the completed temporary record buffer with:

```text
spectrogram wave 300
spectrogram record
spectrogram record 0
```

Record channel `-1` (the default) downmixes stereo; `0` and `1` select the
left or right channel. Both sources preserve their stored amplitudes, and the
record command respects its current start offset and end trim. Spectrograms
embed the source title and axis cues using an Atari-vector-inspired stroke
font with distinct uppercase and lowercase glyphs, so labels remain crisp
without a platform font dependency. Built-in spectrogram and waveform labels
use lowercase. Labels are retained as stroke descriptions and drawn after the
image is scaled, so resizing a window does not enlarge rasterized text. The
display reports frame count, RMS and peak levels in dBFS,
DC offset, dominant spectral peak, centroid, bandwidth, 85% rolloff, and
spectral flatness. The Skred bridge queries stored wavetable rates through the
clean `W* wave,param` result and uses the active audio rate for record buffers,
so frequency metrics are normally shown in hertz. The reusable API falls back
to Nyquist-normalized (`nyq`) values when no rate is supplied.

Use the same sources for a conventional x/y waveform plot:

```text
waveform wave 300
waveform record
waveform record 1
```

Waveform labels sit outside the plot and report the frame count and stored
amplitude range, peak-to-peak level, RMS, peak dBFS, DC offset, crest factor,
and zero-crossing rate. Available loop markers also report their frame range,
duration, and position. Although Skred 0.52.0 does not attach metadata to its
foreign-function data call, skrepl obtains wavetable loop boundaries with
`W* wave,3` and `W* wave,4` before transferring the samples. Record-buffer
transfers are already trimmed, so their original `W-` start/end bounds are not
misrepresented as loops over the returned data.

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

## Voice topology diagrams

Pikchr 1.0.0 is vendored under `third_party/pikchr/`. Display the link graph
implied by Skred's `/vg` command in a dedicated, resizable window with:

```text
topology 8
topology 8 4
/vg 8
```

The optional depth limits traversal; zero or omission walks the complete
reachable graph. Entering a valid `/vg` command retains its normal text output
and also refreshes the diagram. The front end requests Skred's versioned line protocol,
groups multiple relationships between the same voices, and labels pitch, gate,
amplitude, frequency, pan, phase, and ring modulation links. The root voice is
highlighted. FLTK renders Pikchr's SVG geometry, while the view overlays its
`<text>` labels because NanoSVG does not render SVG text.

CTest covers SVG loading, spectrogram rendering and spectral metrics, waveform
rendering and audio metrics, vector-font labels, and voice-graph conversion
through the vendored Pikchr renderer:

```sh
ctest --test-dir build --output-on-failure
```

## Integration overview

The public C API is declared under `include/repl/`; implementation files are
under `src/`. `repl_set_fallback_handler()` lets an embedding application keep
a small set of registered GUI commands while routing every other original
line to its own language runtime. `examples/skred/main.c` demonstrates this
pattern, copies input into the mutable buffer required by `skred_command()`,
prints `skred_log()`, and shuts down the Skred dispatcher and audio engine
when the application exits.
