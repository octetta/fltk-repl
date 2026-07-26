/*
 * font_picker.h - Public C API for fltk-repl GUI font picker dialog.
 *
 * Provides an interactive desktop font chooser dialog with live text preview,
 * monospace filtering, font size spinner, and one-click application to the REPL.
 */

#ifndef FONT_PICKER_H
#define FONT_PICKER_H

#include "repl_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct font_picker_win font_picker_win;

/* Callback when user applies a new font in the font chooser dialog */
typedef void (*font_picker_apply_fn)(const char *font_name, int size, void *userdata);

/* Open or focus the GUI font picker window. */
font_picker_win *repl_font_picker_open(repl_ctx *ctx);

/* Close the font picker window. */
void repl_font_picker_close(font_picker_win *win);

/* Set callback handler when user clicks Apply or OK in the font picker. */
void repl_font_picker_set_handler(font_picker_win *win, font_picker_apply_fn fn, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* FONT_PICKER_H */
