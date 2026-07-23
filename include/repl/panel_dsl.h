#ifndef PANEL_DSL_H
#define PANEL_DSL_H

/*
 * panel_dsl: small declarative GUI panels (sliders/fields/toggles/buttons/
 * choices/button grids) that fire Skred commands, loaded from a tiny text
 * DSL.
 *
 * DSL example:
 *
 *   window "Envelope" 320 260
 *
 *   slider attack  0 5000 10 ms   "env_attack_set %d"  =1200
 *   slider cutoff  20 20000 Hz    "filter_cutoff_set %d"  ~live
 *   row
 *     toggle hardstop           "loop_release_hard_stop %d"  =on
 *     field  voices  1 32       "voice_count_set %d"         =4
 *   endrow
 *   choice wave  "sine saw square"   "voice_wave_set %s"  =saw
 *   label "-- pads --"
 *   grid 2 4
 *     button k1 "note_on 60" "note_off 60"
 *     button k2 "note_on 62" "note_off 62"
 *     button k3 "note_on 64" "note_off 64"
 *   endgrid
 *   button panic "all_notes_off"
 *
 * Grammar (one statement per line, '#' starts a comment, blank lines ignored):
 *
 *   window <"title"> <width> <height>
 *   slider <name> <min> <max> [<step>] [<unit>] <"template"> [=default] [@weight] [~live]
 *   field  <name> <min> <max> <"template"> [=default] [@weight]
 *   toggle <name> <"template"> [=default] [@weight]
 *   button <name> <"template">                        (release-only)
 *   button <name> <"press"> <"release">                (modal; no default)
 *   choice <name> <"space separated options"> <"template"> [=default] [@weight]
 *   label  <"text"> [@weight]                          (no default)
 *   row
 *   ...items...
 *   endrow
 *   grid <rows> <cols>
 *   ...button statements only...
 *   endgrid
 *
 * `<"template">` is a command line sent verbatim to the registered command
 * handler, except:
 *   %d  -> replaced with the widget's value as an integer
 *   %f  -> replaced with the widget's value as a float (%.3f)
 *   %s  -> replaced with the widget's string value (choice only)
 * Only the first occurrence in the template is substituted; that's enough
 * for "one control -> one command" bindings, which covers the intended use.
 *
 * `@weight` (optional, default 1) controls relative width within a `row`.
 *
 * `=default` (optional) sets the widget's initial value instead of the
 * built-in fallback (slider: midpoint of min/max; field: min; toggle:
 * off; choice: first option). Validated at load time -- out-of-range
 * slider/field defaults, unrecognized toggle values (accepts
 * 0/1/on/off/true/false), or a choice default that isn't one of the
 * listed options are all reported as parse errors rather than silently
 * clamped or ignored. `@weight`, `=default`, and `~flag` modifiers may
 * appear in any order after the template.
 *
 * SLIDER STEP AND LIVE MODE
 *
 * A slider may have an optional numeric <step> token between <max> and
 * the (also optional) <unit>: `slider cutoff 20 20000 10 Hz "..."`. It's
 * distinguished from <unit> by looking numeric, so old files with just a
 * unit (`slider attack 0 5000 ms "..."`) keep parsing exactly as before.
 * Without a step, the slider is continuous (FLTK's default granularity).
 *
 * By default, a slider only sends its command on mouse release, to avoid
 * flooding the command handler with one call per pixel of drag. Adding
 * the `~live` flag makes it also send (throttled to roughly one message
 * per 30ms) while dragging, in addition to a guaranteed final message on
 * release:
 *
 *   slider cutoff 20 20000 Hz "filter_cutoff_set %d" ~live
 *
 * MODAL BUTTONS
 *
 * `button name "template"` behaves as before: fires the template once,
 * on release, with no substitution.
 *
 * `button name "press_template" "release_template"` is modal/momentary:
 * `press_template` fires the moment the button is pressed down,
 * `release_template` fires when it's released -- useful for a held gate,
 * e.g. `button k1 "note_on 60" "note_off 60"`. The release message
 * always fires on mouse-up, even if the cursor was dragged off the
 * button first, so a held control can't get stuck "on" with no matching
 * "off". Buttons don't accept `=default` (there's no state to default).
 *
 * BUTTON GRIDS
 *
 * `grid <rows> <cols>` / `endgrid` lays out only `button` statements (any
 * other statement type inside is a load error) into an evenly-spaced
 * R x C grid, filled row-major. Fewer buttons than `rows*cols` leaves the
 * remaining cells blank; more is a load error. Every button in the grid
 * expands to exactly fill its cell, with its label centered on both axes
 * (Fl_Button's default alignment). `grid` is a top-level construct like
 * `row` -- it can't nest inside a `row`, and a `row`/another `grid`
 * can't nest inside it.
 *
 * Windows are live-resizable: sliders, fields, toggles, buttons, choices,
 * and button-grid cells all track the window's width as it's resized
 * (recomputed from the same column-splitting logic used at load time).
 * Row heights and vertical layout stay fixed -- only horizontal space is
 * redistributed. A minimum window size is enforced so controls can't be
 * crushed by shrinking too far.
 *
 * Threading: like bitmap_win, all calls must happen on the FLTK main
 * thread (i.e. from the same context skred_command() normally runs on).
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct panel_win panel_win_t;

/* Command handler: called with a fully-substituted command line whenever a
 * widget fires. Wire this to skred_command() (or a thin wrapper around it)
 * once, globally, before loading any panels. */
typedef void (*panel_command_fn)(const char *line, void *user_data);
void panel_set_command_handler(panel_command_fn fn, void *user_data);

/* Parse `path` and build a new panel window. Returns NULL and prints a
 * diagnostic to stderr (with line number) on a parse error. */
panel_win_t *panel_load_file(const char *path);

/* Same, but from an in-memory DSL string (e.g. embedded default panels). */
panel_win_t *panel_load_string(const char *dsl_text, const char *fallback_title);

/* Tear down and rebuild a panel's contents in place from `path`, keeping
 * the same window (position preserved; size follows the new `window`
 * directive). Returns 0 on success, -1 on parse error (existing panel is
 * left untouched). */
int panel_reload_file(panel_win_t *pw, const char *path);

void panel_show(panel_win_t *pw);
void panel_hide(panel_win_t *pw);
void panel_destroy(panel_win_t *pw);

/* Named panel registry (optional convenience layer). The functions
 * above already fully support multiple independent panels -- every
 * panel_load_file()/panel_load_string() call is its own instance, with
 * no hidden single-panel limit. This registry is just a convenience for
 * managing several named panels (e.g. "envelope", "pads", "mixer") from
 * a command dispatcher without hand-rolling a name -> panel_win_t map. */
panel_win_t *panel_registry_load(const char *name, const char *path); /* loads/replaces the named slot */
panel_win_t *panel_registry_get(const char *name);                     /* NULL if not loaded */
int panel_registry_reload(const char *name);                           /* reload from the path used at load */
void panel_registry_show(const char *name);
void panel_registry_hide(const char *name);
void panel_registry_destroy(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* PANEL_DSL_H */
