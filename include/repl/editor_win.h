/*
 * editor_win.h - Public C API for fltk-repl built-in text editor window.
 *
 * Provides a lightweight embedded desktop text editor with line numbers,
 * file load/save dialogs, syntax-friendly monospace font, and one-click
 * or Ctrl+Enter / Cmd+Enter evaluation of scripts into the REPL session.
 */

#ifndef EDITOR_WIN_H
#define EDITOR_WIN_H

#include "repl_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct editor_win editor_win;

/* Callback function signature when the user evaluates script text */
typedef void (*editor_eval_fn)(const char *code, void *userdata);

/* Open or focus the text editor window. */
editor_win *repl_editor_open(repl_ctx *ctx);

/* Open or focus the text editor window with a specific file path. */
editor_win *repl_editor_open_file(repl_ctx *ctx, const char *filepath);

/* Close the text editor window. */
void repl_editor_close(editor_win *win);

/* Set a custom evaluation callback for the editor window. If not set,
 * evaluating code dispatches lines directly to the host REPL session. */
void repl_editor_set_eval_handler(editor_win *win, editor_eval_fn fn, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* EDITOR_WIN_H */
