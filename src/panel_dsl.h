#ifndef PANEL_DSL_H
#define PANEL_DSL_H

/*
 * panel_dsl: small declarative GUI panels (sliders/fields/toggles/buttons/
 * choices) that fire Skred commands, loaded from a tiny text DSL.
 *
 * DSL example:
 *
 *   window "Envelope" 320 200
 *
 *   slider attack  0 5000 ms   "env_attack_set %d"
 *   slider decay   0 5000 ms   "env_decay_set %d"
 *   row
 *     toggle hardstop           "loop_release_hard_stop %d"
 *     field  voices  1 32       "voice_count_set %d"
 *   endrow
 *   choice wave  "sine saw square"   "voice_wave_set %s"
 *   button panic                "all_notes_off"
 *
 * Grammar (one statement per line, '#' starts a comment, blank lines ignored):
 *
 *   window <"title"> <width> <height>
 *   slider <name> <min> <max> [<unit>] <"template"> [@weight]
 *   field  <name> <min> <max> <"template"> [@weight]
 *   toggle <name> <"template"> [@weight]
 *   button <name> <"template"> [@weight]
 *   choice <name> <"space separated options"> <"template"> [@weight]
 *   label  <"text"> [@weight]
 *   row
 *   ...items...
 *   endrow
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

#ifdef __cplusplus
}
#endif

#endif /* PANEL_DSL_H */
