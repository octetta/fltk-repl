#ifndef BITMAP_WIN_H
#define BITMAP_WIN_H

/*
 * bitmap_win: a small, optional graphics-output window for Skred commands.
 *
 * Usage from a command handler:
 *
 *   bitmap_win_t *bw = bitmap_win_get("scope");   // name is a label/key;
 *                                                  // today all names alias
 *                                                  // to one window, but the
 *                                                  // call site never changes
 *   bitmap_win_set_gray(bw, buf, w, h);
 *   bitmap_win_show(bw);
 *
 * Thread-safety: all calls must happen on the FLTK main thread (i.e. from
 * inside skred_command()/the fallback handler dispatch path). If a command
 * can ever be triggered from a non-FLTK thread (e.g. a scheduled/deferred
 * Skred event), don't call these directly -- marshal through Fl::awake()
 * instead. See bitmap_win.cpp for a stub you can wire that into.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bitmap_win bitmap_win_t;

/* Get (creating on first use) the bitmap window registered under `name`.
 * Right now this always returns the single default window regardless of
 * `name`, except that `name` is used as the window title. Call sites should
 * still pass a meaningful name (e.g. "scope", "spectrogram") so that when a
 * real per-name registry is added later, no call site needs to change. */
bitmap_win_t *bitmap_win_get(const char *name);

void bitmap_win_show(bitmap_win_t *bw);
void bitmap_win_hide(bitmap_win_t *bw);
int  bitmap_win_visible(const bitmap_win_t *bw);

/* Replace the displayed image. Data is copied internally, so the caller's
 * buffer can be reused/freed immediately after the call returns. */
void bitmap_win_set_rgb(bitmap_win_t *bw, const uint8_t *rgb, int w, int h);
void bitmap_win_set_gray(bitmap_win_t *bw, const uint8_t *gray, int w, int h);

void bitmap_win_clear(bitmap_win_t *bw);
void bitmap_win_set_title(bitmap_win_t *bw, const char *title);

#ifdef __cplusplus
}
#endif

#endif /* BITMAP_WIN_H */
