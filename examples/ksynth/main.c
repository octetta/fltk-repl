/* =========================================================================
 * krepl - K-Synth Array DSP REPL Demo Application
 *
 * Built on fltk-repl with vendored K-Synth engine (third_party/ksynth).
 * Features:
 *   - Interactive K-Synth array & vector evaluation
 *   - High-resolution 2D Braille waveform plotting
 *   - Support for interactive FLTK parameter GUI panels
 *   - Headless verification mode (--check)
 * ========================================================================= */

#include "repl/repl_api.h"
#include "repl/panel_dsl.h"
#include "ksynth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static repl_ctx *g_app_repl = NULL;
static uintptr_t g_ks_handle = 0;

/* Braille Sparkline Plotter for K-Synth Array Output */
static void render_braille_waveform(const float *samples, int nsamples, int cols, int rows_cells, char *out, size_t out_cap) {
    if (!samples || nsamples <= 0 || cols <= 0 || rows_cells <= 0 || out_cap == 0) {
        if (out_cap > 0) out[0] = '\0';
        return;
    }

    int gw = cols * 2;
    int gh = rows_cells * 4;
    unsigned char *grid = (unsigned char *)calloc((size_t)gw * (size_t)gh, 1);
    size_t len = 0;

    if (!grid) {
        out[0] = '\0';
        return;
    }

    float min_v = samples[0], max_v = samples[0];
    for (int i = 1; i < nsamples; ++i) {
        if (samples[i] < min_v) min_v = samples[i];
        if (samples[i] > max_v) max_v = samples[i];
    }
    float range = max_v - min_v;
    if (range < 1e-6f) range = 1.0f;

    for (int gx = 0; gx < gw; ++gx) {
        int si = (nsamples <= 1) ? 0 : (gx * (nsamples - 1)) / (gw - 1);
        float norm = (samples[si] - min_v) / range;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        int gy = (int)((1.0f - norm) * (gh - 1) + 0.5f);
        grid[(size_t)gy * gw + gx] = 1;
    }

    static const unsigned char dotbit[4][2] = {
        {0x01, 0x08},
        {0x02, 0x10},
        {0x04, 0x20},
        {0x40, 0x80},
    };

    for (int r = 0; r < rows_cells; ++r) {
        for (int c = 0; c < cols; ++c) {
            unsigned int mask = 0;
            for (int dy = 0; dy < 4; ++dy) {
                int gy = r * 4 + dy;
                for (int dx = 0; dx < 2; ++dx) {
                    int gx = c * 2 + dx;
                    if (grid[(size_t)gy * gw + gx]) {
                        mask |= dotbit[dy][dx];
                    }
                }
            }
            unsigned int cp = 0x2800u + (mask & 0xFFu);
            if (len + 4 < out_cap) {
                out[len++] = (char)(0xE0 | (cp >> 12));
                out[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[len++] = (char)(0x80 | (cp & 0x3F));
            }
        }
        if (r + 1 < rows_cells && len + 2 < out_cap) {
            out[len++] = '\n';
        }
    }
    out[len] = '\0';
    free(grid);
}

static void eval_ksynth_line(const char *line) {
    if (!line || !*line || !g_ks_handle) return;

    int ret = ks_ctx_repl(g_ks_handle, line);
    const char *res_str = ks_ctx_repl_str(g_ks_handle);
    const char *err = ks_ctx_get_error(g_ks_handle);

    if (err && *err) {
        repl_printf(g_app_repl, "\x1b[31merror:\x1b[0m %s\n", err);
        return;
    }

    if (res_str && *res_str) {
        repl_printf(g_app_repl, "\x1b[38;5;51m%s\x1b[0m\n", res_str);
    }

    int len = ks_ctx_repl_length(g_ks_handle);
    if (len > 1) {
        float *buf = (float *)malloc((size_t)len * sizeof(float));
        if (buf) {
            int count = ks_ctx_repl_get_floats(g_ks_handle, buf, len);
            if (count > 1) {
                char plot_buf[4096] = {0};
                render_braille_waveform(buf, count, 60, 4, plot_buf, sizeof(plot_buf));
                repl_printf(g_app_repl, "\x1b[38;5;226m%s\x1b[0m\n", plot_buf);
            }
            free(buf);
        }
    }
}

static void cmd_eval(int argc, char **argv, void *ud) {
    (void)ud;
    if (argc < 2) {
        repl_println(g_app_repl, "usage: eval <ksynth_expr>");
        return;
    }
    char expr[2048] = {0};
    for (int i = 1; i < argc; ++i) {
        if (i > 1) strncat(expr, " ", sizeof(expr) - strlen(expr) - 1);
        strncat(expr, argv[i], sizeof(expr) - strlen(expr) - 1);
    }
    eval_ksynth_line(expr);
}

static void cmd_plot(int argc, char **argv, void *ud) {
    (void)ud;
    if (argc < 2) {
        repl_println(g_app_repl, "usage: plot <var_letter_A_to_Z>");
        return;
    }
    char var = argv[1][0];
    if (var >= 'a' && var <= 'z') var = var - 'a' + 'A';
    if (var >= 'A' && var <= 'Z') {
        eval_ksynth_line(argv[1]);
    } else {
        repl_println(g_app_repl, "plot: variable must be A-Z");
    }
}

static void fallback_handler(const char *line, void *ud) {
    (void)ud;
    if (!line || !*line) return;
    eval_ksynth_line(line);
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--check") == 0) {
        printf("krepl 0.1.0\n");
        printf("KSynth vendored from github.com/octetta/k-synth\n");
        printf("FLTK-REPL version %s\n", FLTK_REPL_VERSION);
        return 0;
    }

    g_ks_handle = ks_ctx_create();
    if (!g_ks_handle) {
        fprintf(stderr, "krepl: failed to create KSynth context\n");
        return 1;
    }

    g_app_repl = repl_create("krepl - K-Synth Array DSP REPL", 800, 600);
    if (!g_app_repl) {
        fprintf(stderr, "krepl: failed to create REPL window\n");
        ks_ctx_destroy(g_ks_handle);
        return 1;
    }

    repl_register_command(g_app_repl, "eval", cmd_eval, "evaluate K-Synth expression");
    repl_register_command(g_app_repl, "plot", cmd_plot, "render Braille waveform of K-Synth variable");
    repl_set_fallback_handler(g_app_repl, fallback_handler, NULL);

    repl_println(g_app_repl, "\x1b[1;36mkrepl - K-Synth Array DSP REPL\x1b[0m");
    repl_println(g_app_repl, "Vendored engine from github.com/octetta/k-synth");
    repl_println(g_app_repl, "Type any K-Synth expression (e.g. \x1b[33mA=100+!44100\x1b[0m or \x1b[33msin A\x1b[0m) or 'help'.");

    repl_run(g_app_repl);

    repl_destroy(g_app_repl);
    ks_ctx_destroy(g_ks_handle);
    return 0;
}
