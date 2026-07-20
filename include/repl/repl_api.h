/*
 * repl_api.h - Pure C API for a cross-platform FLTK terminal/REPL widget.
 *
 * You should never need to write or read C++ to use this library.
 * Everything below is plain C: an opaque handle, function pointers,
 * and functions that take/return simple types (char*, int, etc).
 *
 * Typical usage (see examples/demo/main.c for a full example):
 *
 *     repl_ctx *r = repl_create("My REPL", 900, 600);
 *     repl_register_default_commands(r);
 *     repl_register_command(r, "hello", cmd_hello, NULL);
 *     repl_println(r, "Type 'help' to list commands.");
 *     return repl_run(r);
 *
 * Where cmd_hello looks like:
 *
 *     void cmd_hello(int argc, char **argv, void *userdata) {
 *         (void)argc; (void)argv; (void)userdata;
 *         repl_println(r_global, "hi there!");
 *     }
 */
#ifndef REPL_API_H
#define REPL_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle to a REPL window/session. */
typedef struct repl_ctx repl_ctx;

/*
 * Command callback. argv[0] is the command name itself (like a normal
 * argv), argv[1..argc-1] are the whitespace-separated arguments the
 * user typed. Quoting with "..." or '...' is supported so arguments
 * containing spaces can be passed through as a single argv entry.
 *
 * argv memory is only valid for the duration of the callback; copy
 * anything you need to keep.
 */
typedef void (*repl_cmd_fn)(int argc, char **argv, void *userdata);

typedef enum {
    REPL_THEME_LIGHT = 0,
    REPL_THEME_DARK  = 1
} repl_theme;

/* ---- lifecycle ---------------------------------------------------- */

/* Create the REPL window. Call exactly once per session. */
repl_ctx *repl_create(const char *title, int width, int height);

/* Destroy the REPL window and free all resources. */
void repl_destroy(repl_ctx *ctx);

/* Enter the FLTK event loop. Blocks until the window is closed or
 * repl_quit() is called. Returns the same value Fl::run() would. */
int repl_run(repl_ctx *ctx);

/* Programmatically close the window / exit repl_run(). */
void repl_quit(repl_ctx *ctx);

/* ---- commands ------------------------------------------------------ */

/* Register a command the user can type by name. Re-registering the
 * same name replaces the previous handler. */
void repl_register_command(repl_ctx *ctx, const char *name,
                            repl_cmd_fn fn, void *userdata);

/* Remove a previously registered command, if present. */
void repl_unregister_command(repl_ctx *ctx, const char *name);

/* Registers a small set of convenience builtins:
 *   help            - lists all registered commands
 *   clear           - clears the scrollback
 *   theme light|dark
 *   font <name> <size>
 *   quit / exit
 * Safe to call even if you only want some of these; you can
 * repl_unregister_command() any you don't want afterward. */
void repl_register_default_commands(repl_ctx *ctx);

/* ---- output --------------------------------------------------------- */

/* Append raw UTF-8 text to the scrollback (no trailing newline added).
 * Safe to call from within a command callback. Full Unicode is
 * supported, including Braille pattern characters (U+2800-U+28FF)
 * commonly used for terminal waveform/spectrogram displays. */
void repl_print(repl_ctx *ctx, const char *utf8_text);

/* Same as repl_print but appends a trailing newline. */
void repl_println(repl_ctx *ctx, const char *utf8_text);

/* printf-style convenience wrapper around repl_print. */
void repl_printf(repl_ctx *ctx, const char *fmt, ...);

/* Clear all scrollback text. */
void repl_clear(repl_ctx *ctx);

/* Change the prompt string shown before each input line (default "> "). */
void repl_set_prompt(repl_ctx *ctx, const char *prompt);

/* ---- history ---------------------------------------------------------
 * Up/Down arrow recall is built in automatically. These are exposed in
 * case you want to seed or export history. */

int repl_history_count(repl_ctx *ctx);
/* Returns a pointer valid until the next history mutation; do not free. */
const char *repl_history_get(repl_ctx *ctx, int index_from_oldest);
void repl_history_clear(repl_ctx *ctx);

/* ---- appearance ------------------------------------------------------- */

void repl_set_theme(repl_ctx *ctx, repl_theme theme);
repl_theme repl_get_theme(repl_ctx *ctx);

/* Fine-grained color override, 0xRRGGBB. Call after repl_set_theme()
 * if you want to tweak individual colors rather than use the theme
 * defaults. */
void repl_set_colors(repl_ctx *ctx,
                      unsigned int bg_rgb,
                      unsigned int fg_rgb,
                      unsigned int prompt_rgb,
                      unsigned int input_rgb);

/* Set the font by name (e.g. "Courier", "DejaVu Sans Mono", "Menlo").
 * Returns 1 on success, 0 if the font name could not be matched to an
 * installed font (current font is left unchanged). A monospace font
 * with good Unicode/Braille coverage is recommended -- see README. */
int repl_set_font(repl_ctx *ctx, const char *font_name, int size);
void repl_set_font_size(repl_ctx *ctx, int size);

/* Fills buf (caller-allocated, capacity max_names) with newline-
 * separated names of all fonts FLTK found on the system, for building
 * a font picker. Returns the number of fonts found. */
int repl_list_fonts(repl_ctx *ctx, char *buf, int buf_capacity);

/* ---- dialogs ------------------------------------------------------------ */

/* Native file open/save dialogs. filter is an FLTK-style pattern
 * string, e.g. "Text\t*.txt\nAll Files\t*". Pass NULL for no filter.
 * Returns a malloc'd UTF-8 path (free with repl_free_string) or NULL
 * if the user cancelled. */
char *repl_open_file_dialog(repl_ctx *ctx, const char *title, const char *filter);
char *repl_save_file_dialog(repl_ctx *ctx, const char *title, const char *filter);
char *repl_choose_directory_dialog(repl_ctx *ctx, const char *title);
void  repl_free_string(char *s);

#ifdef __cplusplus
}
#endif

#endif /* REPL_API_H */
