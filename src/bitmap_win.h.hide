#ifndef BITMAP_WIN_H
#define BITMAP_WIN_H

/*
 * bitmap_win: a small, optional graphics-output window for Skred commands.
 *
 * Usage from a command handler:
 *
 *   bitmap_win_t *bw = bitmap_win_get("scope");   // name is the window key
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

/* Get or create the bitmap window registered under `name`. */
bitmap_win_t *bitmap_win_get(const char *name);

void bitmap_win_show(bitmap_win_t *bw);
void bitmap_win_hide(bitmap_win_t *bw);
void bitmap_win_hide_all(void);
int  bitmap_win_visible(const bitmap_win_t *bw);

/* Replace the displayed image. Data is copied internally, so the caller's
 * buffer can be reused/freed immediately after the call returns. */
void bitmap_win_set_rgb(bitmap_win_t *bw, const uint8_t *rgb, int w, int h);
void bitmap_win_set_gray(bitmap_win_t *bw, const uint8_t *gray, int w, int h);

int bitmap_win_set_spectrogram(bitmap_win_t *bw, const float *samples,
                               int frames, int channels, int channel,
                               int width, int height);
int bitmap_win_set_spectrogram_labeled(bitmap_win_t *bw, const float *samples,
                                       int frames, int channels, int channel,
                                       int width, int height,
                                       const char *title);
int bitmap_win_set_spectrogram_labeled_ex(bitmap_win_t *bw,
                                          const float *samples, int frames,
                                          int channels, int channel, int width,
                                          int height, const char *title,
                                          float sample_rate);
int bitmap_win_set_waveform(bitmap_win_t *bw, const float *samples,
                            int frames, int channels, int channel,
                            int width, int height, const char *title,
                            int loop_start, int loop_end);
int bitmap_win_set_waveform_ex(bitmap_win_t *bw, const float *samples,
                               int frames, int channels, int channel,
                               int width, int height, const char *title,
                               int loop_start, int loop_end,
                               float sample_rate);

void bitmap_win_clear(bitmap_win_t *bw);
void bitmap_win_set_title(bitmap_win_t *bw, const char *title);

#ifdef __cplusplus
}
#endif

#endif /* BITMAP_WIN_H */
