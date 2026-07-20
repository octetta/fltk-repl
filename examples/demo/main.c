/*
 * Demo for the fltk-repl library. This file is plain C -- no C++
 * knowledge needed. It shows:
 *   - registering your own commands
 *   - printing output (repl_print/println/printf)
 *   - native open/save file dialogs
 *   - rendering a Unicode Braille waveform/spectrogram, the kind of
 *     thing a DSP/audio API might hand back as a quick terminal-style
 *     visualization
 *
 * Build: see the top-level README.md. In short:
 *     cmake -B build
 *     cmake --build build -j
 *     ./build/repl_demo
 */
#include "repl/repl_api.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REPL_DEMO_PI 3.14159265358979323846

static repl_ctx *g_ctx = NULL;

/* ---- tiny UTF-8 helper for Braille Pattern codepoints (U+2800-28FF) -- */

static void utf8_append_braille(char *buf, size_t *len, unsigned int mask) {
    unsigned int cp = 0x2800u + (mask & 0xFFu);
    buf[(*len)++] = (char)(0xE0 | (cp >> 12));
    buf[(*len)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[(*len)++] = (char)(0x80 | (cp & 0x3F));
}

/*
 * Renders `nsamples` values (each expected in [0,1]) as a block of
 * Braille "sparkline" characters `cols` wide and `rows_cells` tall.
 * Each character cell packs a 2x4 dot grid, giving 2x the horizontal
 * and 4x the vertical resolution of plain ASCII art per character --
 * the same trick terminal waveform/spectrogram viewers use.
 */
static void braille_plot(const double *values, int nsamples,
                          int cols, int rows_cells,
                          char *out, size_t out_cap) {
    int gw = cols * 2;
    int gh = rows_cells * 4;
    unsigned char *grid = (unsigned char *)calloc((size_t)gw * (size_t)gh, 1);
    size_t len = 0;

    if (!grid || out_cap == 0) {
        if (out_cap) out[0] = '\0';
        free(grid);
        return;
    }

    for (int gx = 0; gx < gw; ++gx) {
        int si = (nsamples <= 1) ? 0 : (gx * (nsamples - 1)) / (gw - 1);
        double v = values[si];
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;
        int gy = (int)((1.0 - v) * (gh - 1) + 0.5);
        grid[(size_t)gy * gw + gx] = 1;
    }

    /* dot numbering per Unicode Braille Patterns block:
     *   1 4
     *   2 5
     *   3 6
     *   7 8
     * bit(dotN) = 1 << (N-1)
     */
    static const unsigned char dotbit[4][2] = {
        {0x01, 0x08},
        {0x02, 0x10},
        {0x04, 0x20},
        {0x40, 0x80},
    };

    for (int cy = 0; cy < rows_cells; ++cy) {
        for (int cx = 0; cx < cols; ++cx) {
            unsigned int mask = 0;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    int gx = cx * 2 + sx;
                    int gy = cy * 4 + sy;
                    if (grid[(size_t)gy * gw + gx]) mask |= dotbit[sy][sx];
                }
            }
            if (len + 4 >= out_cap) goto done;
            utf8_append_braille(out, &len, mask);
        }
        if (len + 2 >= out_cap) goto done;
        out[len++] = '\n';
    }
done:
    out[len] = '\0';
    free(grid);
}

/* ---- commands ------------------------------------------------------- */

static void cmd_wave(int argc, char **argv, void *ud) {
    (void)ud;
    int n = 200;
    double values[200];
    double freq = (argc >= 2) ? atof(argv[1]) : 3.0;

    for (int i = 0; i < n; ++i) {
        double t = (double)i / (double)(n - 1);
        values[i] = 0.5 + 0.5 * sin(2.0 * REPL_DEMO_PI * freq * t);
    }

    char buf[8192];
    braille_plot(values, n, 70, 8, buf, sizeof(buf));
    repl_print(g_ctx, buf);
}

static void cmd_noise(int argc, char **argv, void *ud) {
    (void)argc;
    (void)argv;
    (void)ud;
    int n = 140;
    double values[140];
    for (int i = 0; i < n; ++i) {
        values[i] = (double)rand() / (double)RAND_MAX;
    }

    char buf[8192];
    braille_plot(values, n, 70, 6, buf, sizeof(buf));
    repl_print(g_ctx, buf);
}

static void cmd_open(int argc, char **argv, void *ud) {
    (void)argc;
    (void)argv;
    (void)ud;
    char *path = repl_open_file_dialog(g_ctx, "Open a file", "All Files\t*");
    if (!path) {
        repl_println(g_ctx, "(cancelled)");
        return;
    }
    repl_printf(g_ctx, "selected: %s\n", path);
    repl_free_string(path);
}

static void cmd_save(int argc, char **argv, void *ud) {
    (void)argc;
    (void)argv;
    (void)ud;
    char *path = repl_save_file_dialog(g_ctx, "Save a file", "Text\t*.txt\nAll Files\t*");
    if (!path) {
        repl_println(g_ctx, "(cancelled)");
        return;
    }
    repl_printf(g_ctx, "would save to: %s\n", path);
    repl_free_string(path);
}

static void cmd_echo(int argc, char **argv, void *ud) {
    (void)ud;
    for (int i = 1; i < argc; ++i) {
        repl_print(g_ctx, argv[i]);
        if (i + 1 < argc) repl_print(g_ctx, " ");
    }
    repl_print(g_ctx, "\n");
}

int main(void) {
    srand((unsigned)time(NULL));

    g_ctx = repl_create("fltk-repl demo", 960, 620);
    repl_register_default_commands(g_ctx);

    repl_register_command(g_ctx, "wave", cmd_wave, NULL);
    repl_register_command(g_ctx, "noise", cmd_noise, NULL);
    repl_register_command(g_ctx, "open", cmd_open, NULL);
    repl_register_command(g_ctx, "save", cmd_save, NULL);
    repl_register_command(g_ctx, "echo", cmd_echo, NULL);

    repl_println(g_ctx,
        "Try: help | wave [freq] | noise | open | save | theme light|dark | "
        "font <name> <size> | echo <text>");

    int rc = repl_run(g_ctx);
    repl_destroy(g_ctx);
    return rc;
}
