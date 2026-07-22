# Panel DSL Reference

A small declarative language for building control-panel windows —
sliders, fields, toggles, buttons, dropdowns, button grids — that fire
Skred commands directly. Loaded from a plain text file via
`panel_load_file()`, or from an in-memory string via
`panel_load_string()`.

Not a general UI toolkit: no expressions, no nesting beyond one level of
row/grid grouping, no scripting. One statement, one line, one widget. If
you find yourself wanting computed values or conditionals, that's a
deliberate boundary of this DSL rather than a missing feature — see
[Non-goals](#non-goals).

---

## Quick example

```
window "Envelope" 320 320

slider attack  0 5000 10 ms  "env_attack_set %d"  =1200
slider cutoff  20 20000 Hz   "filter_cutoff_set %d"  ~live
row
  toggle hardstop           "loop_release_hard_stop %d"  =on
  field  voices  1 32       "voice_count_set %d"         =4
endrow
choice wave  "sine saw square"   "voice_wave_set %s"  =saw
label "-- pads --"
grid 2 4
  button k1 "note_on 60" "note_off 60"
  button k2 "note_on 62" "note_off 62"
  button k3 "note_on 64" "note_off 64"
endgrid
button panic "all_notes_off"
```

This produces a window with two full-width sliders (one with a step
size, one in live-drag mode), a row containing a checkbox and a numeric
field side by side, a dropdown, a bold section label, a 2×4 grid of
momentary note-gate buttons, and a panic button — each control wired to
a specific Skred command.

---

## File structure

- One statement per line.
- `#` starts a comment (to end of line). Blank lines are ignored.
- Statements are whitespace-tokenized, except `"quoted strings"`, which
  are treated as a single token (quotes stripped, `\"` escapes a literal
  quote inside one).
- Every statement type is documented below with its exact grammar.

---

## `window` — required, first statement

```
window <"title"> <width> <height>
```

```
window "Envelope" 320 240
```

Sets the window title and starting size. If the panel's content doesn't
fit in `height`, the window grows automatically to fit (width is
respected as given). See [Resizing](#resizing) for what happens after
the window is shown.

---

## `slider` — continuous numeric control

```
slider <name> <min> <max> [<step>] [<unit>] <"template"> [=default] [@weight] [~live]
```

```
slider attack 0 5000 "env_attack_set %d"
slider attack 0 5000 ms "env_attack_set %d"
slider attack 0 5000 10 ms "env_attack_set %d"
slider cutoff 20 20000 Hz "filter_cutoff_set %d" =800 ~live
```

- `name` — shown as the label above the slider.
- `min` / `max` — slider bounds.
- `step` — optional; sets the slider's granularity (`Fl_Slider::step()`).
  Can be fractional (e.g. `0.01`). Without it, the slider is continuous.
- `unit` — optional; if present, appended to the label in parentheses,
  e.g. `attack (ms)`.
- `step` and `unit` are told apart automatically by whether the token
  looks numeric — so `slider attack 0 5000 ms "..."` (old-style, unit
  only) and `slider attack 0 5000 10 "..."` (step only, no unit) both
  parse correctly with no ambiguity. At most one of each is allowed;
  two numeric-looking tokens or two non-numeric tokens before the
  template is a load error.
- `template` — command sent with `%d`/`%f` replaced by the slider's
  value. See [Templates](#templates).
- `=default` — optional starting value; must fall within `[min, max]`
  or the file fails to load (see [Validation](#validation-and-errors)).
  Without it, the slider starts at the midpoint of `min`/`max`.
- `~live` — optional flag; see [Update timing](#update-timing) for what
  it changes.

By default, fires on **mouse release** only. With `~live`, also fires
(throttled) while dragging — see [Update timing](#update-timing).

---

## `field` — numeric text entry

```
field <name> <min> <max> <"template"> [=default] [@weight]
```

```
field voices 1 32 "voice_count_set %d" =4
```

A labeled integer text box. `min`/`max` currently set the default value
range for validation purposes and the fallback default (`min`) — typed
input isn't clamped as you type, so keep the command handler on the
Skred side defensive about out-of-range values regardless.

Fires on **Enter** or when the field loses focus.

---

## `toggle` — checkbox

```
toggle <name> <"template"> [=default] [@weight]
```

```
toggle hardstop "loop_release_hard_stop %d" =on
```

Sends `0` or `1` via `%d` in the template. `=default` accepts
`0`/`1`/`on`/`off`/`true`/`false` (case-insensitive). Without a default,
starts unchecked.

---

## `button` — momentary trigger

```
button <name> <"template">                          (release-only)
button <name> <"press_template"> <"release_template"> (modal)
```

```
button panic "all_notes_off"
button gate  "note_on 60" "note_off 60"
```

With one template, behaves as a normal button: fires the template
**verbatim** on release, no substitution, since a button carries no
value. This is the release-only form.

With two templates, the button is **modal** (momentary): the first
fires the instant it's pressed down, the second fires when it's
released. Useful for a held gate — press to start a note, release to
stop it. The release message always fires on mouse-up, even if the
mouse was dragged off the button before releasing, so a held control
can't get stuck "on" with no matching "off" — this is deliberately
different from a normal button's usual "only counts as a click if you
release inside it" behavior.

Does **not** accept `=default` (there's no state to default) in either
form; a file that tries will fail to load with a clear error rather
than being silently ignored.

---

## `choice` — dropdown

```
choice <name> <"space separated options"> <"template"> [=default] [@weight]
```

```
choice wave "sine saw square" "voice_wave_set %s" =saw
```

Options are a single quoted string, split on whitespace. The chosen
option's text replaces `%s` in the template. `=default` must exactly
match one of the listed options, or the file fails to load. Without a
default, starts on the first option.

---

## `label` — static text

```
label <"text"> [@weight]
```

```
label "-- danger zone --"
```

Purely cosmetic — bold, left-aligned text, no interactivity, no
template, no command ever fires. Useful as a section header between
groups of controls. Does not accept `=default`.

---

## `row` / `endrow` — horizontal grouping

By default every statement is its own full-width row, stacked
vertically. Wrap statements in `row` / `endrow` to place them side by
side instead:

```
row
  toggle hardstop  "loop_release_hard_stop %d"
  field  voices  1 32  "voice_count_set %d"
endrow
```

Rows don't nest — a `row` inside a `row` is a load error. Each row's
available width is split across its items by weight (see below); row
height is the tallest item type in that row.

---

## `grid` / `endgrid` — button grids

```
grid <rows> <cols>
  button ...
  button ...
endgrid
```

```
grid 2 4
  button k1 "note_on 60" "note_off 60"
  button k2 "note_on 62" "note_off 62"
  button k3 "note_on 64" "note_off 64"
endgrid
```

Lays out `button` statements only into an evenly spaced `rows` × `cols`
grid, filled row-major (left to right, top to bottom). Every cell is the
same size; every button expands to exactly fill its cell, with its label
centered on both axes. On window resize, columns track the window's
width the same way row items do — see [Resizing](#resizing).

- Fewer buttons than `rows * cols` leaves the remaining cells blank —
  useful when building a grid up incrementally.
- More buttons than `rows * cols` is a load error.
- Only `button` statements are allowed inside a `grid`; any other
  statement type is a load error.
- `grid` is a top-level construct like `row` — it can't be nested inside
  a `row`, and a `row` (or another `grid`) can't be nested inside it.

---

## Modifiers: `@weight`, `=default`, `~flag`

Three optional trailing modifiers can follow a statement's positional
arguments (after the template, where there is one), in **any order**:

```
field  voices  1 32  "voice_count_set %d"  @2
slider cutoff 20 20000 Hz "filter_cutoff_set %d" =800 ~live
slider cutoff 20 20000 Hz "filter_cutoff_set %d" ~live =800
```

- **`@weight`** — relative column width. Default is `1` for every item.
  Within a `row` (or `grid` column), available width is divided
  proportionally by weight — an `@2` item gets twice the width of an
  `@1` item in the same row. Outside a `row`, weight has no visible
  effect (a solo item already takes the full row width).
- **`=default`** — sets the widget's initial value. See the table below.
- **`~flag`** — a bare boolean flag, e.g. `~live` (sliders only). Using
  an unsupported flag on a statement type that doesn't recognize it is a
  load error, not a silent no-op — see
  [Validation](#validation-and-errors).

`=default` accepted values and fallbacks without one:

| Type     | Accepts             | Fallback without `=default` |
|----------|----------------------|------------------------------|
| slider   | number in `[min,max]`| midpoint of `min`/`max`     |
| field    | integer in `[min,max]`| `min`                       |
| toggle   | `0`/`1`/`on`/`off`/`true`/`false` | unchecked (`0`) |
| choice   | one of the listed options | first option            |
| button   | *(not allowed)*       | —                            |
| label    | *(not allowed)*       | —                            |

---

## Templates

The `"template"` string on each interactive statement is the command
line sent to whatever handler was registered with
`panel_set_command_handler()` — normally straight into `skred_command()`.

- `%d` → the widget's value as an integer.
- `%f` → the widget's value as a float, formatted to 3 decimal places by
  default. For control over precision, use printf-style `%.Nf` instead,
  e.g. `%.0f` (no decimals), `%.1f`, `%.6f`. Only the precision digits
  are special-cased -- no other printf flags (width, `+`, etc.) apply.
  Requests above 17 decimal places are clamped to 17 (beyond a double's
  useful precision) rather than overflowing. (If a numeric-valued
  widget's template has no `%f`/`%.Nf`, `%d` is tried instead.)
- `%s` → the widget's string value (choice only).
- Only the **first** occurrence is substituted. This DSL binds one
  control to one command; if you need a command built from multiple
  widget values, that's outside what this format is for.
- `button` templates are sent as-is, with no substitution at all.

```
slider attack 0 5000 ms "env_attack_set %d"
```

firing at value `1800` sends exactly:

```
env_attack_set 1800
```

---

## Update timing

- **Sliders** fire on mouse release by default, not per-pixel while
  dragging — this keeps a fast drag from flooding the command handler
  with hundreds of calls. Add the `~live` flag to also fire while
  dragging, throttled to roughly one message per 30ms, with a
  guaranteed final message on release regardless of throttle timing (so
  the release value is never lost or stale).
- **Fields** fire on Enter or on losing focus.
- **Toggles, choices** fire immediately on interaction.
- **Buttons** (release-only form) fire on release. **Modal buttons**
  (two-template form) fire their press template on press and their
  release template on release — see [`button`](#button--momentary-trigger).

---

## Resizing

Panel windows are live-resizable. Dragging the window wider or narrower
reflows every slider, field, toggle, button, and choice horizontally to
match, using the same weighted column split described under
[Modifiers](#modifiers-weight-default-flag). Button grid columns track
width the same way, keeping every cell equal-width. Row heights and
vertical layout are fixed — only horizontal space redistributes. A
minimum window size is enforced so controls can't be crushed by
shrinking too far.

Vertical space does **not** currently adapt (rows/grid rows don't grow
taller, and extra height beyond the content doesn't get redistributed)
— that's a deliberate scope cut, not an oversight, since "which rows get
the extra height" is a real design choice rather than a mechanical one.

---

## Validation and errors

The whole file is validated before any window is built — a broken file
fails to load cleanly (`panel_load_file`/`panel_load_string` return
`NULL`) rather than partially constructing a window. Errors are printed
to `stderr` with a line number, for example:

```
panel_dsl: line 4: default 99 is outside slider's range [0, 10]
panel_dsl: line 7: toggle default must be 0/1/on/off, got 'maybe'
panel_dsl: line 9: choice default 'zzz' is not one of the listed options
panel_dsl: line 12: button does not take a default value
panel_dsl: line 3: unterminated row (missing endrow)
panel_dsl: line 2: unknown slider flag '~bogus'
panel_dsl: line 2: field does not support flag '~live'
panel_dsl: line 8: grid declares 2x2 = 4 cells but 5 buttons were given
panel_dsl: line 3: grid may only contain button statements (got 'slider')
panel_dsl: line 3: nested grid not supported
```

---

## Full worked example

```
window "Voice" 360 420

label "-- pitch --"
slider transpose -24 24 st "voice_transpose_set %d" =0
row
  toggle glide       "voice_glide_set %d"        =off @1
  field  glide_ms  0 2000 "voice_glide_ms_set %d" =150 @2
endrow

label "-- filter --"
slider cutoff  20 20000 Hz "filter_cutoff_set %d"   =2000 ~live
slider resonance 0 1 0.01 "filter_res_set %.2f"     =0.20
choice filter_type "lowpass highpass bandpass" "filter_type_set %s" =lowpass

label "-- output --"
row
  field  gain_db -60 12 "voice_gain_set %d" =0
  toggle mute            "voice_mute_set %d" =off
endrow

label "-- pads --"
grid 2 4
  button k1 "note_on 60" "note_off 60"
  button k2 "note_on 62" "note_off 62"
  button k3 "note_on 64" "note_off 64"
  button k4 "note_on 65" "note_off 65"
endgrid

button panic "all_notes_off"
```

---

## Non-goals

These are intentionally out of scope for this DSL — not missing
features, but boundaries kept in place to keep the format small and
predictable:

- **No expressions or computed values.** A widget's value goes into
  exactly one template; there's no way to combine two widgets into one
  command from the DSL side.
- **No conditionals or scripting.** The file describes layout and
  bindings, nothing else.
- **No reverse binding.** Widgets show DSL-authored/default values, not
  the engine's live current parameter value — there's no query-and-sync
  mechanism.
- **No nested grouping.** One level of horizontal grouping (`row` or
  `grid`) is enough for the panel sizes this is meant for; deeper
  nesting, or mixing `row`/`grid` inside each other, would need a real
  layout engine, which is explicitly not what this is.

---

## Reloading

```c
panel_win_t *pw = panel_load_file("controls.pnl");
panel_show(pw);
/* ...later, after editing controls.pnl on disk... */
panel_reload_file(pw, "controls.pnl");
```

`panel_reload_file()` re-parses the file and rebuilds the panel's
widgets in place, keeping the same window. If the edited file has a
parse error, the existing panel is left untouched and the error is
printed to `stderr` — a bad edit during a live session can't leave you
with a half-rebuilt or blank panel.
