/*
 * FLTK front end for the packaged PULP/Skred release API.
 *
 * Skred owns the command language. The FLTK REPL handles only its small set
 * of GUI commands and forwards every otherwise unknown line, unchanged, to
 * skred_command().
 */
#include "repl/repl_api.h"
#include "repl/repl_prefs.h"
#include "repl/bitmap_win.h"
#include "repl/panel_dsl.h"
#include "repl/foreign_bridge.h"
#include "SpectrogramBridge.h"
#include "TopologyWindow.h"
#include <skred/api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __has_include(<skred/skred_vfs.h>)
#include <skred/skred_vfs.h>
#define HAS_SKRED_VFS 1
#endif

#ifndef FLTK_REPL_VERSION
#define FLTK_REPL_VERSION "unknown"
#endif
#ifndef FLTK_REPL_BUILD_DATE
#define FLTK_REPL_BUILD_DATE "unknown"
#endif
#ifndef FLTK_REPL_FLTK_VERSION
#define FLTK_REPL_FLTK_VERSION "unknown"
#endif
#ifndef FLTK_REPL_PIKCHR_VERSION
#define FLTK_REPL_PIKCHR_VERSION "unknown"
#endif

#define SKRED_URL "https://github.com/octetta/pulp"
#define SKREPL_GITHUB_URL "https://github.com/octetta/fltk-repl"
#define OCTETTA_YOUTUBE_URL "https://www.youtube.com/@octetta"
#define OCTETTA_LINKEDIN_URL "https://www.linkedin.com/in/octetta"

/* miniaudio is part of the linked Skred library and exports its runtime
 * version even though its full implementation header is not packaged. */
extern const char *ma_version_string(void);

typedef struct app_state {
    repl_ctx *repl;
} app_state;

/* Persistent settings for boot command */
static unsigned int g_voices = 32;
static unsigned int g_frames = 128;
static int g_port = 0;
static int g_output = -1;
static int g_input = -1;

static void usage(const char *program) {
    printf("usage: %s [options]\n", program);
    printf("  -v, --voices N       voice count (default 32)\n");
    printf("  -r, --frames N       requested audio frames (default 128)\n");
    printf("  -p, --port N         UDP control port (default 0; disabled)\n");
    printf("  -o, --output N       playback device index (default -1)\n");
    printf("  -i, --input N        capture device index (-2 disables capture)\n");
    printf("      --check           print linked release information and exit\n");
    printf("  -h, --help            show this help\n");
}

static int parse_int(const char *option, const char *text, int *out) {
    char *end = NULL;
    long value;
    if (!text || !*text) {
        fprintf(stderr, "%s requires a number\n", option);
        return 0;
    }
    value = strtol(text, &end, 0);
    if (!end || *end != '\0') {
        fprintf(stderr, "invalid value for %s: %s\n", option, text);
        return 0;
    }
    *out = (int)value;
    return 1;
}

static void parse_ys_dump_line(const char *line);

static void print_skred_log_silent(app_state *app) {
    (void)app;
    char *log = skred_log();
    if (log && *log) {
        char *copy = strdup(log);
        if (copy) {
            char *saveptr = NULL;
            char *tok = strtok_r(copy, "\r\n", &saveptr);
            while (tok) {
                parse_ys_dump_line(tok);
                tok = strtok_r(NULL, "\r\n", &saveptr);
            }
            free(copy);
        }
    }
}

static void print_skred_log(app_state *app) {
    char *log = skred_log();
    if (log && *log) {
        char *copy = strdup(log);
        if (copy) {
            char *saveptr = NULL;
            char *tok = strtok_r(copy, "\r\n", &saveptr);
            while (tok) {
                parse_ys_dump_line(tok);
                tok = strtok_r(NULL, "\r\n", &saveptr);
            }
            free(copy);
        }
        repl_print(app->repl, log);
    }
}

static const char *miniaudio_version(void) {
    const char *version = ma_version_string();
    return version && *version ? version : "unknown";
}

static void print_banner_stdout(void) {
    printf("     _                 _ \n");
    printf(" ___| | ___ __ ___  __| |\n");
    printf("/ __| |/ / '__/ _ \\/ _  |\n");
    printf("\\__ \\   <| | |  __/ (_| |\n");
    printf("|___/_|\\_\\_|  \\___|\\__,_|\n");
}

static void print_banner_repl(app_state *app) {
  repl_println(app->repl, " ⢀⣀ ⡇⡠ ⡀⣀ ⢀⡀ ⣀⡀ ⡇");
  repl_println(app->repl, " ⠭⠕ ⠏⠢ ⠏  ⠣⠭ ⡧⠜ ⠣");
}

static void ex_print_banner_repl(app_state *app) {
    repl_println(app->repl, "      _                  _ ");
    repl_println(app->repl, " ____| | ___ __ ____  __| |");
    repl_println(app->repl, "/ ___| |/ / '__/ __ \\/ _  |");
    repl_println(app->repl, "\\___ \\   <| | |  ___/ (_| |");
    repl_println(app->repl, "|____/_|\\_\\_|  \\____|\\__,_|");
}

static void print_release_info_stdout(void) {
    print_banner_stdout();
    printf("Copyright (c) 2023-2525 octetta\n");
    printf("skrepl %s\n", FLTK_REPL_VERSION);
    printf("Built %s\n", FLTK_REPL_BUILD_DATE);
    printf("Skred %s\n", skred_version());
    printf("source: " SKREPL_GITHUB_URL "\n");
    printf("PULP/skred: " SKRED_URL "\n");
    printf("YouTube: " OCTETTA_YOUTUBE_URL "\n");
    printf("LinkedIn: " OCTETTA_LINKEDIN_URL "\n");
    printf("%s\n", skred_features());
}

static void print_release_info_repl(app_state *app) {
    print_banner_repl(app);
    repl_printf(app->repl, "Copyright (c) 2023-2525 octetta\n");
    repl_printf(app->repl, "skrepl %s using SKRED %s\n", FLTK_REPL_VERSION, skred_version());
    repl_printf(app->repl, "Built %s\n", FLTK_REPL_BUILD_DATE);
    repl_println(app->repl, "source: " SKREPL_GITHUB_URL);
    repl_println(app->repl, "YouTube: " OCTETTA_YOUTUBE_URL);
    repl_println(app->repl, "LinkedIn: " OCTETTA_LINKEDIN_URL);
    repl_printf(app->repl, "%s\n", skred_features());
}

static void skred_line(const char *line, void *userdata) {
    app_state *app = (app_state *)userdata;
    size_t length = strlen(line);
    char *command = (char *)malloc(length + 1);
    int result;

    if (!command) {
        repl_println(app->repl, "skred-repl: out of memory");
        return;
    }
    memcpy(command, line, length + 1);
    result = skred_command(command);
    free(command);

    print_skred_log(app);
    if (result < 0) repl_quit(app->repl);
}

static void print_panel_list_cb(const char *name, const char *path, const char *params, int is_shown, void *user_data) {
    app_state *app = (app_state *)user_data;
    if (params && params[0]) {
        repl_printf(app->repl, "  '%s' -> %s (%s, %s)\n", name, path, params, is_shown ? "visible" : "hidden");
    } else {
        repl_printf(app->repl, "  '%s' -> %s (%s)\n", name, path, is_shown ? "visible" : "hidden");
    }
}

static void print_panel_control_cb(const char *control_name, const char *val_str, void *user_data) {
    app_state *app = (app_state *)user_data;
    repl_printf(app->repl, "  %s = %s\n", control_name, val_str);
}

static void bitmap_panel_handler(const char *line, void *userdata) {
    app_state *app = (app_state *)userdata;
    char cmd[64] = {0}, arg[256] = {0};
    int n = sscanf(line, "%63s %255[^\n]", cmd, arg);

    if (strcmp(cmd, "pacman") == 0 || strcmp(cmd, "waka") == 0 || strcmp(cmd, "ghosts") == 0) {
        /* Line 0: Top Domes & Pellets */
        repl_println(app->repl,
            "\x1b[38;5;226m⢀⣴⣾⣿⣿⣷⣦⡀\x1b[0m  "                  /* Pac-Man Line 0 (Yellow #226) */
            "⠀⠀⠀⠀⠀⠀⠀⠀  "                                           /* Dot Line 0 */
            "\x1b[38;5;231m⠀⠀⠀⠀⠀⠀⠀⠀\x1b[0m  "                  /* Power Pellet Line 0 (White #231) */
            "⠀⠀⠀⠀⠀⠀⠀⠀  "                                           /* Dot Line 0 */
            "\x1b[38;5;196m⢀⣴⣾⣿⣿⣷⣦⡀\x1b[0m  "                 /* Blinky Line 0 (Red #196) */
            "\x1b[38;5;201m⢀⣴⣾⣿⣿⣷⣦⡀\x1b[0m  "                 /* Pinky Line 0 (Pink #201) */
            "\x1b[38;5;51m⢀⣴⣾⣿⣿⣷⣦⡀\x1b[0m  "                  /* Inky Line 0 (Cyan #51) */
            "\x1b[38;5;208m⢀⣴⣾⣿⣿⣷⣦⡀\x1b[0m");               /* Clyde Line 0 (Orange #208) */

        /* Line 1: Middles & Eyes (Exactly 8 chars per sprite) */
        repl_println(app->repl,
            "\x1b[38;5;226m⣾⣿⣿⣿⣿⠿⠛⠁\x1b[0m  "                  /* Pac-Man Line 1 */
            "\x1b[38;5;220m⠀⠀⠀⣤⣤⠀⠀⠀\x1b[0m  "                  /* Dot Line 1 (Gold #220) */
            "\x1b[38;5;231m⠀⠀⣾⣿⣿⣷⠀⠀\x1b[0m  "                  /* Power Pellet Line 1 */
            "\x1b[38;5;220m⠀⠀⠀⣤⣤⠀⠀⠀\x1b[0m  "                  /* Dot Line 1 */
            "\x1b[38;5;196m⣾⣿\x1b[38;5;231m⠏⠏\x1b[38;5;196m⣿\x1b[38;5;231m⠏⠏\x1b[38;5;196m⣷\x1b[0m  " /* Blinky Line 1 (8 chars) */
            "\x1b[38;5;201m⣾⣿\x1b[38;5;231m⠏⠏\x1b[38;5;201m⣿\x1b[38;5;231m⠏⠏\x1b[38;5;201m⣷\x1b[0m  " /* Pinky Line 1 (8 chars) */
            "\x1b[38;5;51m⣾⣿\x1b[38;5;231m⠏⠏\x1b[38;5;51m⣿\x1b[38;5;231m⠏⠏\x1b[38;5;51m⣷\x1b[0m  "  /* Inky Line 1 (8 chars) */
            "\x1b[38;5;208m⣾⣿\x1b[38;5;231m⠏⠏\x1b[38;5;208m⣿\x1b[38;5;231m⠏⠏\x1b[38;5;208m⣷\x1b[0m"); /* Clyde Line 1 (8 chars) */

        /* Line 2: Lower Body */
        repl_println(app->repl,
            "\x1b[38;5;226m⢿⣿⣿⣿⣿⣶⣤⡀\x1b[0m  "                  /* Pac-Man Line 2 */
            "\x1b[38;5;220m⠀⠀⠀⠛⠛⠀⠀⠀\x1b[0m  "                  /* Dot Line 2 */
            "\x1b[38;5;231m⠀⠀⢿⣿⣿⡿⠀⠀\x1b[0m  "                  /* Power Pellet Line 2 */
            "\x1b[38;5;220m⠀⠀⠀⠛⠛⠀⠀⠀\x1b[0m  "                  /* Dot Line 2 */
            "\x1b[38;5;196m⣿⣿⣿⣿⣿⣿⣿⣿\x1b[0m  "                 /* Blinky Line 2 */
            "\x1b[38;5;201m⣿⣿⣿⣿⣿⣿⣿⣿\x1b[0m  "                 /* Pinky Line 2 */
            "\x1b[38;5;51m⣿⣿⣿⣿⣿⣿⣿⣿\x1b[0m  "                  /* Inky Line 2 */
            "\x1b[38;5;208m⣿⣿⣿⣿⣿⣿⣿⣿\x1b[0m");               /* Clyde Line 2 */

        /* Line 3: Bottom Skirts & Jaws */
        repl_println(app->repl,
            "\x1b[38;5;226m⠈⠻⢿⣿⣿⡿⠟⠁\x1b[0m  "                  /* Pac-Man Line 3 */
            "⠀⠀⠀⠀⠀⠀⠀⠀  "                                           /* Dot Line 3 */
            "\x1b[38;5;231m⠀⠀⠀⠀⠀⠀⠀⠀\x1b[0m  "                  /* Power Pellet Line 3 */
            "⠀⠀⠀⠀⠀⠀⠀⠀  "                                           /* Dot Line 3 */
            "\x1b[38;5;196m⡟⠙⢿⠋⠙⡿⠋⢻\x1b[0m  "                 /* Blinky Line 3 */
            "\x1b[38;5;201m⡟⠙⢿⠋⠙⡿⠋⢻\x1b[0m  "                 /* Pinky Line 3 */
            "\x1b[38;5;51m⡟⠙⢿⠋⠙⡿⠋⢻\x1b[0m  "                  /* Inky Line 3 */
            "\x1b[38;5;208m⡟⠙⢿⠋⠙⡿⠋⢻\x1b[0m");               /* Clyde Line 3 */
        return;
    }

    if (strcmp(cmd, "bitmap") == 0) {
        bitmap_win_t *bw = bitmap_win_get("default");
        if (n >= 2 && strcmp(arg, "show") == 0) { bitmap_win_show(bw); return; }
        if (n >= 2 && strcmp(arg, "hide") == 0) { bitmap_win_hide(bw); return; }
        if (n >= 2 && strcmp(arg, "clear") == 0) { bitmap_win_clear(bw); return; }
        bitmap_win_show(bw);
        return;
    }

    if (strcmp(cmd, "spectrogram") == 0) {
        int value;
        char extra;
        if (n >= 2 && strncmp(arg, "scale ", 6) == 0) {
            const char *s = arg + 6;
            if (strcmp(s, "log") == 0) {
                bitmap_win_set_spectrogram_scale(SPECTROGRAM_SCALE_LOG);
                repl_println(app->repl, "Spectrogram frequency scale set to Logarithmic.");
            } else {
                bitmap_win_set_spectrogram_scale(SPECTROGRAM_SCALE_LINEAR);
                repl_println(app->repl, "Spectrogram frequency scale set to Linear.");
            }
            return;
        }
        if (n >= 2 && (strncmp(arg, "cmap ", 5) == 0 || strncmp(arg, "colormap ", 9) == 0)) {
            const char *c = (strncmp(arg, "cmap ", 5) == 0) ? arg + 5 : arg + 9;
            if (strcmp(c, "viridis") == 0) bitmap_win_set_spectrogram_colormap(SPECTROGRAM_COLORMAP_VIRIDIS);
            else if (strcmp(c, "crt") == 0 || strcmp(c, "green") == 0) bitmap_win_set_spectrogram_colormap(SPECTROGRAM_COLORMAP_CRT_GREEN);
            else if (strcmp(c, "amber") == 0) bitmap_win_set_spectrogram_colormap(SPECTROGRAM_COLORMAP_AMBER);
            else if (strcmp(c, "gray") == 0 || strcmp(c, "mono") == 0) bitmap_win_set_spectrogram_colormap(SPECTROGRAM_COLORMAP_GRAYSCALE);
            else bitmap_win_set_spectrogram_colormap(SPECTROGRAM_COLORMAP_MAGMA);
            repl_printf(app->repl, "Spectrogram colormap set to '%s'.\n", c);
            return;
        }
        if (n >= 2 && sscanf(arg, "wave %d %c", &value, &extra) == 1 &&
            value >= 0) {
            int result = skred_spectrogram_wave(value);
            print_skred_log(app);
            if (result != 0)
                repl_println(app->repl, "No wavetable data available.");
            return;
        }
        if (n >= 2 && strcmp(arg, "record") == 0) {
            int result = skred_spectrogram_record(-1);
            print_skred_log(app);
            if (result != 0)
                repl_println(app->repl, "No completed recording data available.");
            return;
        }
        if (n >= 2 && sscanf(arg, "record %d %c", &value, &extra) == 1 &&
            value >= -1 && value <= 1) {
            int result = skred_spectrogram_record(value);
            print_skred_log(app);
            if (result != 0)
                repl_println(app->repl, "No completed recording data available.");
            return;
        }
        repl_println(app->repl,
            "usage: spectrogram wave <slot> | record [-1|0|1] | scale [linear|log] | cmap [magma|viridis|crt|amber|gray]");
        return;
    }

    if (strcmp(cmd, "waveform") == 0) {
        int value;
        char extra;
        if (n >= 2 && strncmp(arg, "trigger ", 8) == 0) {
            const char *t = arg + 8;
            int on = (strcmp(t, "1") == 0 || strcmp(t, "on") == 0 || strcmp(t, "true") == 0) ? 1 : 0;
            bitmap_win_set_waveform_trigger(on);
            repl_printf(app->repl, "Oscilloscope zero-crossing trigger set to %s.\n", on ? "ON" : "OFF");
            return;
        }
        if (n >= 2 && sscanf(arg, "wave %d %c", &value, &extra) == 1 &&
            value >= 0) {
            int result = skred_waveform_wave(value);
            print_skred_log(app);
            if (result != 0)
                repl_println(app->repl, "No wavetable data available.");
            return;
        }
        if (n >= 2 && strcmp(arg, "record") == 0) {
            int result = skred_waveform_record(-1);
            print_skred_log(app);
            if (result != 0)
                repl_println(app->repl, "No completed recording data available.");
            return;
        }
        if (n >= 2 && sscanf(arg, "record %d %c", &value, &extra) == 1 &&
            value >= -1 && value <= 1) {
            int result = skred_waveform_record(value);
            print_skred_log(app);
            if (result != 0)
                repl_println(app->repl, "No completed recording data available.");
            return;
        }
        repl_println(app->repl,
            "usage: waveform wave <slot> | record [-1|0|1] | trigger [on|off]");
        return;
    }

    if (strcmp(cmd, "topology") == 0) {
        int voice = -1;
        int depth = 0;
        char extra;
        char error[256];
        int parsed = n >= 2 ? sscanf(arg, "%d %d %c", &voice, &depth, &extra) : 0;
        if ((parsed == 1 || parsed == 2) && voice >= 0 && depth >= 0) {
            if (topology_show_voice(voice, depth, error, sizeof(error)) != 0)
                repl_printf(app->repl, "Topology failed: %s\n", error);
            return;
        }
        repl_println(app->repl, "usage: topology <voice> [depth]");
        return;
    }

    if (strcmp(cmd, "/vg") == 0) {
        int voice = -1;
        int format = 0;
        int depth = 0;
        char extra;
        char error[256];
        int parsed = 0;
        if (n >= 2 &&
            sscanf(arg, "%d , %d , %d %c", &voice, &format, &depth, &extra) == 3) {
            parsed = 3;
        } else {
            voice = -1; format = 0; depth = 0;
            if (n >= 2 &&
                sscanf(arg, "%d , %d %c", &voice, &format, &extra) == 2) {
                parsed = 2;
            } else {
                voice = -1; format = 0;
                if (n >= 2 && sscanf(arg, "%d %c", &voice, &extra) == 1)
                    parsed = 1;
            }
        }
        skred_line(line, userdata);
        if (parsed >= 1 && parsed <= 3 && voice >= 0 &&
            (format == 0 || format == 1) && depth >= 0 &&
            topology_show_voice(voice, depth, error, sizeof(error)) != 0) {
            repl_printf(app->repl, "Topology failed: %s\n", error);
        }
        return;
    }

    if (strcmp(cmd, "panel") == 0) {
        char pname[64] = {0}, rest[192] = {0};
        int n2 = (n >= 2) ? sscanf(arg, "%63s %191[^\n]", pname, rest) : 0;

        if (n2 >= 2 && strcmp(pname, "load") == 0) {
            char panel_name[64] = {0}, path[192] = {0}, params[192] = {0};
            int consumed = 0;
            /* "panel load <name> <path> [key=value ...]" -- any tokens
             * after <path> are space-separated key=value pairs (e.g.
             * "voice=1"), joined with commas to match
             * panel_registry_load_params()'s "key=value,key2=value2"
             * syntax. With no trailing pairs this behaves exactly as
             * before (plain panel_registry_load()). */
            int n3 = sscanf(rest, "%63s %191s%n", panel_name, path, &consumed);
            if (n3 == 2) {
                const char *p = rest + consumed;
                while (*p == ' ') p++;
                if (*p) {
                    char token[64];
                    int first = 1, tconsumed = 0;
                    while (sscanf(p, "%63s%n", token, &tconsumed) == 1) {
                        if (!first) strncat(params, ",", sizeof(params) - strlen(params) - 1);
                        strncat(params, token, sizeof(params) - strlen(params) - 1);
                        first = 0;
                        p += tconsumed;
                        while (*p == ' ') p++;
                        if (!*p) break;
                    }
                }

                panel_win_t *p2 = params[0]
                    ? panel_registry_load_params(panel_name, path, params)
                    : panel_registry_load(panel_name, path);
                if (p2) {
                    panel_registry_show(panel_name);
                } else {
                    repl_println(app->repl, "Panel load failed.");
                }
            } else {
                repl_println(app->repl, "usage: panel load <name> <file.pnl> [key=value ...]");
            }
            return;
        }
        if (n2 >= 2 && strcmp(pname, "reload") == 0) {
            if (panel_registry_reload(rest) == 0) {
                repl_println(app->repl, "Panel reloaded successfully.");
            } else {
                repl_println(app->repl, "Panel reload failed (unknown panel or invalid file).");
            }
            return;
        }
        if (n2 >= 1 && strcmp(pname, "open") == 0) {
            char panel_name[64] = {0};
            if (n2 >= 2) sscanf(rest, "%63s", panel_name);
            if (!panel_name[0]) strcpy(panel_name, "panel1");

            char *file = repl_open_file_dialog(app->repl, "Select Panel File", "Panel Files (*.pnl)\t*.pnl\nZip Archives (*.zip)\t*.zip\nAll Files (*)\t*");
            if (file) {
                panel_win_t *p = panel_registry_load(panel_name, file);
                if (p) {
                    panel_registry_show(panel_name);
                    repl_printf(app->repl, "Loaded panel '%s' from: %s\n", panel_name, file);
                } else {
                    repl_printf(app->repl, "Failed to load panel from: %s\n", file);
                }
                repl_free_string(file);
            } else {
                repl_println(app->repl, "Panel browse cancelled.");
            }
            return;
        }
        if (n2 >= 2 && strcmp(pname, "set") == 0) {
            char panel_name[64] = {0}, ctrl_name[64] = {0}, val_str[128] = {0};
            int fire_cmd = 0;
            int n3 = sscanf(rest, "%63s %63s %127s %d", panel_name, ctrl_name, val_str, &fire_cmd);
            if (n3 >= 3) {
                if (panel_registry_set_value(panel_name, ctrl_name, val_str, fire_cmd) == 0) {
                    repl_printf(app->repl, "Updated %s.%s = %s\n", panel_name, ctrl_name, val_str);
                } else {
                    repl_printf(app->repl, "Failed to set %s.%s (panel or control not found).\n", panel_name, ctrl_name);
                }
            } else {
                repl_println(app->repl, "usage: panel set <panel_name> <control_name> <value> [fire=0|1]");
            }
            return;
        }
        if (n2 >= 1 && (strcmp(pname, "list") == 0 || strcmp(pname, "ls") == 0)) {
            repl_println(app->repl, "Active Panels:");
            panel_registry_list(print_panel_list_cb, app);
            return;
        }
        if (n2 >= 2 && (strcmp(pname, "dump") == 0 || strcmp(pname, "values") == 0)) {
            char panel_name[64] = {0};
            sscanf(rest, "%63s", panel_name);
            repl_printf(app->repl, "Controls & Values for '%s':\n", panel_name);
            if (panel_registry_enum_values(panel_name, print_panel_control_cb, app) != 0) {
                repl_printf(app->repl, "Panel '%s' not found.\n", panel_name);
            }
            return;
        }
        if (n2 >= 2 && strcmp(pname, "get") == 0) {
            char panel_name[64] = {0}, ctrl_name[64] = {0}, buf[128] = {0};
            int n3 = sscanf(rest, "%63s %63s", panel_name, ctrl_name);
            if (n3 == 2) {
                if (panel_registry_get_value(panel_name, ctrl_name, buf, sizeof(buf)) == 0) {
                    repl_printf(app->repl, "%s.%s = %s\n", panel_name, ctrl_name, buf);
                } else {
                    repl_printf(app->repl, "Control %s.%s not found.\n", panel_name, ctrl_name);
                }
            } else if (n3 == 1) {
                repl_printf(app->repl, "Controls & Values for '%s':\n", panel_name);
                if (panel_registry_enum_values(panel_name, print_panel_control_cb, app) != 0) {
                    repl_printf(app->repl, "Panel '%s' not found.\n", panel_name);
                }
            } else {
                repl_println(app->repl, "usage: panel get <panel_name> [control_name]");
            }
            return;
        }
        if (n2 >= 2 && strcmp(pname, "show") == 0) { panel_registry_show(rest); return; }
        if (n2 >= 2 && strcmp(pname, "hide") == 0) { panel_registry_hide(rest); return; }
        if (n2 >= 2 && (strcmp(pname, "step") == 0 || strcmp(pname, "highlight") == 0)) {
            int step_idx = atoi(rest);
            panel_registry_set_step_highlight(step_idx);
            return;
        }
        repl_println(app->repl, "usage: panel load <name> <file.pnl> [key=val ...] | list | dump <name> | set <name> <ctrl> <val> [fire] | get <name> [ctrl] | step <N> | reload <name> | show <name> | hide <name>");
        return;
    }

    if (strcmp(cmd, "pwd") == 0) {
        char cwd[1024] = {0};
        if (getcwd(cwd, sizeof(cwd))) {
            repl_printf(app->repl, "%s\n", cwd);
        }
        return;
    }

    if (strcmp(cmd, ":clear-history") == 0 || strcmp(cmd, "clear-history") == 0 || strcmp(cmd, "clear_history") == 0) {
        repl_history_clear(app->repl);
        repl_println(app->repl, "Command history cleared.");
        return;
    }

    if (strcmp(cmd, "cd") == 0) {
        if (n < 2) {
            char *dir = repl_choose_directory_dialog(app->repl, "Select Working Directory");
            if (dir) {
                if (chdir(dir) == 0) {
#if HAS_SKRED_VFS
                    if (skred_chdir) skred_chdir(dir);
#endif
                    repl_printf(app->repl, "Working directory set to: %s\n", dir);
                } else {
                    repl_printf(app->repl, "Failed to change working directory to: %s\n", dir);
                }
                repl_free_string(dir);
            }
            return;
        }
        if (chdir(arg) == 0) {
#if HAS_SKRED_VFS
            if (skred_chdir) skred_chdir(arg);
#endif
            char cwd[1024] = {0};
            if (getcwd(cwd, sizeof(cwd))) {
                repl_printf(app->repl, "Working directory set to: %s\n", cwd);
            }
        } else {
            repl_printf(app->repl, "cd: failed to change directory to '%s'\n", arg);
        }
        return;
    }

    if (strcmp(cmd, "browse") == 0) {
        char sub[64] = {0}, arg2[192] = {0};
        int n2 = (n >= 2) ? sscanf(arg, "%63s %191[^\n]", sub, arg2) : 0;

        if (n2 == 0 || strcmp(sub, "dir") == 0 || strcmp(sub, "folder") == 0) {
            char *dir = repl_choose_directory_dialog(app->repl, "Select Working Directory");
            if (dir) {
                if (chdir(dir) == 0) {
#if HAS_SKRED_VFS
                    if (skred_chdir) skred_chdir(dir);
#endif
                    repl_printf(app->repl, "Working directory set to: %s\n", dir);
                } else {
                    repl_printf(app->repl, "Failed to change working directory to: %s\n", dir);
                }
                repl_free_string(dir);
            } else {
                repl_println(app->repl, "Directory browse cancelled.");
            }
            return;
        }

        if (strcmp(sub, "file") == 0) {
            char *file = repl_open_file_dialog(app->repl, "Select File", "All Files\t*");
            if (file) {
                repl_printf(app->repl, "Selected file: %s\n", file);
                repl_free_string(file);
            } else {
                repl_println(app->repl, "File browse cancelled.");
            }
            return;
        }

        if (strcmp(sub, "panel") == 0 || strcmp(sub, "open") == 0) {
            char panel_name[64] = {0};
            if (n2 >= 2) sscanf(arg2, "%63s", panel_name);
            if (!panel_name[0]) strcpy(panel_name, "panel1");

            char *file = repl_open_file_dialog(app->repl, "Select Panel File", "Panel Files (*.pnl)\t*.pnl\nZip Archives (*.zip)\t*.zip\nAll Files (*)\t*");
            if (file) {
                panel_win_t *p = panel_registry_load(panel_name, file);
                if (p) {
                    panel_registry_show(panel_name);
                    repl_printf(app->repl, "Loaded panel '%s' from: %s\n", panel_name, file);
                } else {
                    repl_printf(app->repl, "Failed to load panel from: %s\n", file);
                }
                repl_free_string(file);
            } else {
                repl_println(app->repl, "Panel browse cancelled.");
            }
            return;
        }

        repl_println(app->repl, "usage: browse [dir|folder] | browse file | browse panel [name]");
        return;
    }

    /* Not a GUI command → forward to Skred */
    skred_line(line, userdata);
}

static void cmd_credits(int argc, char **argv, void *userdata) {
    app_state *app = (app_state *)userdata;
    if (app) {
        ex_print_banner_repl(app);
        repl_printf(app->repl, "SKRED %s\n", skred_version());
        repl_printf(app->repl, "FLTK %s\n", FLTK_REPL_FLTK_VERSION);
        repl_printf(app->repl, "miniaudio %s\n", miniaudio_version());
        repl_printf(app->repl, "Pikchr %s\n", FLTK_REPL_PIKCHR_VERSION);
    }
}

static void cmd_boot(int argc, char **argv, void *userdata) {
    app_state *app = (app_state *)userdata;
    int changed = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "voices") == 0 && i+1 < argc) {
            int v = atoi(argv[i+1]);
            if (v >= 1 && v <= 64) {
                g_voices = (unsigned int)v;
                changed = 1;
            }
        } else if (strcmp(argv[i], "frames") == 0 && i+1 < argc) {
            int f = atoi(argv[i+1]);
            if (f >= 1) {
                g_frames = (unsigned int)f;
                changed = 1;
            }
        } else if (strcmp(argv[i], "port") == 0 && i+1 < argc) {
            g_port = atoi(argv[i+1]);
            changed = 1;
        }
    }

    repl_println(app->repl, "Stopping current Skred engine...");
    skred_control_dispatch_stop();
    skred_stop();

    repl_printf(app->repl, "Restarting with voices=%u, frames=%u, port=%d...\n",
                g_voices, g_frames, g_port);

    if (skred_start(g_frames, g_voices, g_port) != 0) {
        repl_println(app->repl, "Failed to restart Skred engine!");
    } else {
        repl_println(app->repl, "Skred engine restarted successfully.");
    }
}

static void gui_help(int argc, char **argv, void *userdata) {
    app_state *app = (app_state *)userdata;
    if (argc >= 2) {
        const char *topic = argv[1];
        if (!strcmp(topic, "edit") || !strcmp(topic, "editor")) {
            repl_println(app->repl,
                "Usage: edit [filepath]\n"
                "  Opens the built-in desktop script text editor window.\n"
                "  Features line numbers, Open/Save file dialogs, dark/light themes, and\n"
                "  one-click or Ctrl+Enter / Cmd+Enter execution into the REPL session.");
            return;
        }
        if (!strcmp(topic, "udp")) {
            repl_println(app->repl,
                "Usage: udp connect <host> <port> | send <text> | mode forward|log|off | color <1..255> | status | disconnect\n"
                "  Attach to external programs or remote instances via UDP.\n"
                "  Incoming UDP responses are displayed in custom ANSI 256 color (default 51 = cyan).");
            return;
        }
        if (!strcmp(topic, "panel")) {
            repl_println(app->repl,
                "Usage: panel load <name> <file.pnl> [key=val ...] | list | get <name> [ctrl] | set <name> <ctrl> <val> [fire=0|1] | dump <name> | reload <name> | show <name> | hide <name>\n"
                "  Manage interactive GUI parameter panels built with the panel DSL.\n"
                "  Sliders support continuous/stepped ranges, ~live drag updates, and ~spring return-to-default.");
            return;
        }
        if (!strcmp(topic, "spectrogram")) {
            repl_println(app->repl,
                "Usage: spectrogram wave <slot> | record [-1|0|1] | scale [linear|log] | cmap [magma|viridis|crt|amber|gray]\n"
                "  Renders floating-point audio spectrograms with spectral peak, centroid, bandwidth, and flatness analysis.");
            return;
        }
        if (!strcmp(topic, "waveform")) {
            repl_println(app->repl,
                "Usage: waveform wave <slot> | record [-1|0|1] | trigger [on|off]\n"
                "  Renders oscilloscope waveforms with peak-to-peak, RMS, dBFS, crest factor, and zero-crossing analysis.");
            return;
        }
        if (!strcmp(topic, "topology")) {
            repl_println(app->repl,
                "Usage: topology <voice> [depth]\n"
                "  Renders interactive Pikchr voice topology link diagrams for Skred synthesis voices.");
            return;
        }
    }

    repl_println(app->repl,
        "GUI Commands:\n"
        "  edit [filepath]                  open script text editor window (Ctrl+Enter to run)\n"
        "  udp connect <host> <port>        attach/send commands via UDP (colored responses)\n"
        "    udp send <text> | mode forward|log|off | color <1..255> | status | disconnect\n"
        "  panel load <name> <file.pnl>     load and display interactive parameter GUI panel\n"
        "    panel list | get | set | dump | reload | show | hide\n"
        "  spectrogram wave|record|scale    render audio spectrogram with spectral metrics\n"
        "  waveform wave|record|trigger     render oscilloscope waveform with audio metrics\n"
        "  topology <voice> [depth]         render Pikchr voice topology diagram\n"
        "  bitmap [show|hide|clear]         toggle standalone graphic output window\n"
        "  theme light|dark|custom|save     switch or save interface color theme\n"
        "  font [\"name\" [size]]             change or view terminal font\n"
        "  cd [path] / pwd / browse         navigation & file chooser dialogs\n"
        "  boot [voices N] [frames N]       restart Skred engine with parameters\n"
        "  clear                            clear scrollback text\n"
        "  credits                          display credits and project links\n"
        "  quit / exit                      stop everything and exit\n"
        "\n"
        "Type 'help <command>' for topic help (e.g. 'help edit', 'help udp', 'help panel').\n"
        "Every other line is sent directly to Skred.");
}

static void cmd_quit(int argc, char **argv, void *userdata) {
    app_state *app = (app_state *)userdata;
    (void)argc;
    (void)argv;
    //skred_foreign_function_clear(9);
    skred_spectrogram_unbind();
    topology_hide();
    bitmap_win_hide_all();
    repl_quit(app->repl);
}

static int s_playhead_step = -1;
static int s_playhead_bpm = 130;
static int s_playhead_subdivision = 16;
static int s_playhead_running = 0;

static void playhead_timer_cb(void *data) {
    (void)data;
    if (!s_playhead_running) return;
    s_playhead_step = (s_playhead_step + 1) % 16;
    panel_registry_set_step_highlight(s_playhead_step);
    double step_sec = 60.0 / ((double)s_playhead_bpm * ((double)s_playhead_subdivision / 4.0));
    if (step_sec < 0.01) step_sec = 0.01;
    repl_add_timeout(step_sec, playhead_timer_cb, NULL);
}

static void panel_to_skred(const char *line, void *user_data) {
    (void)user_data;
    if (!line) return;

    if ((line[0] == 'Z' && line[1] == '1') || (line[0] == 'z' && line[1] == 'q' && line[2] == '1')) {
        s_playhead_running = 1;
        skred_command("yc1");
    } else if (line[0] == 'Z' && line[1] == '0') {
        s_playhead_running = 0;
        panel_registry_set_step_highlight(-1);
    } else if (line[0] == 'M' && (line[1] == ' ' || (line[1] >= '0' && line[1] <= '9'))) {
        int bpm = 120, sub = 16;
        if (sscanf(line, "M %d %d", &bpm, &sub) >= 1) {
            if (bpm > 0) s_playhead_bpm = bpm;
            if (sub > 0) s_playhead_subdivision = sub;
        } else if (sscanf(line, "M%d", &bpm) == 1) {
            if (bpm > 0) s_playhead_bpm = bpm;
        }
    }

    size_t len = strlen(line);
    char *cmd = (char *)malloc(len + 1);
    if (cmd) {
        memcpy(cmd, line, len + 1);
        skred_command(cmd);
        if (strstr(cmd, "/ls")) {
            for (int v = 0; v < 5; ++v) {
                for (int s = 0; s < 16; ++s) {
                    panel_registry_set_grid_step_state(v, s, 0);
                }
            }
            skred_command("ys?");
            if (user_data) {
                print_skred_log_silent((app_state *)user_data);
            }
        } else if (user_data) {
            print_skred_log((app_state *)user_data);
        }
        free(cmd);
    }
}

static void panel_error_to_repl(const char *message, void *user_data) {
    app_state *app = (app_state *)user_data;
    if (app && app->repl && message) repl_println(app->repl, message);
}

static _Thread_local int g_foreign_repl_dispatching;

static void foreign_repl_handler(const char *line, void *user) {
    app_state *app = (app_state *)user;
    if (!app || !app->repl || g_foreign_repl_dispatching) return;
    g_foreign_repl_dispatching = 1;
    repl_dispatch_line(app->repl, line);
    g_foreign_repl_dispatching = 0;
}

static void parse_ys_dump_line(const char *line);

static int fltk_repl_foreign_call(const skred_foreign_call_t *call, void *user) {
    (void)user;
    if (call->string && call->string[0]) {
        parse_ys_dump_line(call->string);
        foreign_bridge_dispatch(call->string);
    }
    return 0;
}

#ifndef SKRED_CONTROL_EVENT_PATTERN_CHANGE
#define SKRED_CONTROL_EVENT_PATTERN_CHANGE 10
#endif
#ifndef SKRED_CONTROL_EVENT_TEMPO_CHANGE
#define SKRED_CONTROL_EVENT_TEMPO_CHANGE 11
#endif
#ifndef SKRED_CONTROL_EVENT_PATTERN_QUEUE
#define SKRED_CONTROL_EVENT_PATTERN_QUEUE 12
#endif
#ifndef SKRED_CONTROL_EVENT_MUTE_CHANGE
#define SKRED_CONTROL_EVENT_MUTE_CHANGE 13
#endif
#ifndef SKRED_CONTROL_EVENT_ERROR
#define SKRED_CONTROL_EVENT_ERROR 14
#endif

static void parse_ys_dump_line(const char *line) {
    if (!line) return;
    const char *xpos = strstr(line, "x");
    if (!xpos) return;
    int step = atoi(xpos + 1);
    if (step < 0 || step >= 16) return;

    if (strstr(line, "[#]") || strstr(line, "[]")) {
        for (int v = 0; v < 5; ++v) {
            panel_registry_set_grid_step_state(v, step, 0);
        }
    } else {
        for (int v = 0; v < 5; ++v) {
            char vtag[8];
            snprintf(vtag, sizeof(vtag), "v%d", v);
            const char *vpos = strstr(line, vtag);
            if (vpos) {
                float vvel = 1.0f;
                const char *lpos = strstr(vpos, "l");
                if (lpos && lpos < xpos) vvel = (float)atof(lpos + 1);
                int state = (vvel >= 0.8f) ? 2 : (vvel > 0.0f ? 1 : 0);
                panel_registry_set_grid_step_state(v, step, state);
            }
        }
    }
}

static void skred_control_event_cb(int fd, void *ud) {
    (void)fd;
    skred_control_event_t events[16];
    int count = skred_control_event_poll(events, 16);
    int last_step = -1;
    for (int i = 0; i < count; ++i) {
        uint32_t type = events[i].type;
        if (type == SKRED_CONTROL_EVENT_PATTERN_STEP && events[i].step >= 0 && events[i].step < 16) {
            if (s_playhead_running) last_step = events[i].step;
        } else if (type == SKRED_CONTROL_EVENT_PATTERN_CHANGE || type == SKRED_CONTROL_EVENT_PATTERN_QUEUE) {
            for (int v = 0; v < 5; ++v) {
                for (int s = 0; s < 16; ++s) {
                    panel_registry_set_grid_step_state(v, s, 0);
                }
            }
            skred_command("ys?");
            if (ud) print_skred_log_silent((app_state *)ud);
        } else if (type == SKRED_CONTROL_EVENT_TEMPO_CHANGE) {
            if (events[i].step > 0) s_playhead_bpm = events[i].step;
        }
    }
    if (last_step >= 0) {
        panel_registry_set_step_highlight(last_step);
    }
}

static void load_appearance_preferences(repl_prefs *prefs, repl_ctx *repl) {
    char theme[16];
    char fontName[256];
    int fontSize;

    repl_prefs_get_string(prefs, "theme", theme, sizeof(theme),
                          repl_get_theme(repl) == REPL_THEME_LIGHT ? "light" : "dark");
    if (strcmp(theme, "light") == 0) {
        repl_set_theme(repl, REPL_THEME_LIGHT);
    } else if (strcmp(theme, "dark") == 0) {
        repl_set_theme(repl, REPL_THEME_DARK);
    } else if (strcmp(theme, "custom") == 0) {
        repl_load_theme_file(repl, NULL);
    }

    repl_prefs_get_string(prefs, "fontname", fontName, sizeof(fontName),
                          repl_get_font_name(repl));
    repl_prefs_get_int(prefs, "fontsize", &fontSize, repl_get_font_size(repl));
    if (fontSize <= 0) fontSize = repl_get_font_size(repl);
    if (!repl_set_font(repl, fontName, fontSize)) {
        fprintf(stderr, "saved font is not available: %s\n", fontName);
    }
}

static void save_appearance_preferences(repl_prefs *prefs, repl_ctx *repl) {
    repl_theme t = repl_get_theme(repl);
    const char *theme = (t == REPL_THEME_LIGHT) ? "light" : ((t == REPL_THEME_DARK) ? "dark" : "custom");

    repl_prefs_set_string(prefs, "theme", theme);
    repl_prefs_set_string(prefs, "fontname", repl_get_font_name(repl));
    repl_prefs_set_int(prefs, "fontsize", repl_get_font_size(repl));
    if (repl_prefs_flush(prefs) < 0) {
        fprintf(stderr, "could not write skrepl preferences\n");
    }
}

int main(int argc, char **argv) {
    unsigned int voices = 32;
    unsigned int frames = 128;
    int port = 0;
    int output = -1;
    int input = -1;
    int check_only = 0;
    int i;
    app_state app;
    repl_prefs *prefs;

    memset(&app, 0, sizeof(app));

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        int value;
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(argv[0]);
            return 0;
        } else if (!strcmp(arg, "--check")) {
            check_only = 1;
        } else if (!strcmp(arg, "-v") || !strcmp(arg, "--voices")) {
            if (++i >= argc || !parse_int(arg, argv[i], &value)) return 2;
            if (value < 1 || value > 64) { fprintf(stderr, "voices must be 1..64\n"); return 2; }
            voices = (unsigned int)value;
        } else if (!strncmp(arg, "-v", 2) && arg[2]) {
            if (!parse_int("-v", arg + 2, &value)) return 2;
            if (value < 1 || value > 64) { fprintf(stderr, "voices must be 1..64\n"); return 2; }
            voices = (unsigned int)value;
        } else if (!strcmp(arg, "-r") || !strcmp(arg, "--frames")) {
            if (++i >= argc || !parse_int(arg, argv[i], &value)) return 2;
            if (value < 1) { fprintf(stderr, "frames must be positive\n"); return 2; }
            frames = (unsigned int)value;
        } else if (!strncmp(arg, "-r", 2) && arg[2]) {
            if (!parse_int("-r", arg + 2, &value)) return 2;
            if (value < 1) { fprintf(stderr, "frames must be positive\n"); return 2; }
            frames = (unsigned int)value;
        } else if (!strcmp(arg, "-p") || !strcmp(arg, "--port")) {
            if (++i >= argc || !parse_int(arg, argv[i], &port)) return 2;
        } else if (!strncmp(arg, "-p", 2) && arg[2]) {
            if (!parse_int("-p", arg + 2, &port)) return 2;
        } else if (!strcmp(arg, "-o") || !strcmp(arg, "--output")) {
            if (++i >= argc || !parse_int(arg, argv[i], &output)) return 2;
        } else if (!strncmp(arg, "-o", 2) && arg[2]) {
            if (!parse_int("-o", arg + 2, &output)) return 2;
        } else if (!strcmp(arg, "-i") || !strcmp(arg, "--input")) {
            if (++i >= argc || !parse_int(arg, argv[i], &input)) return 2;
        } else if (!strncmp(arg, "-i", 2) && arg[2]) {
            if (!parse_int("-i", arg + 2, &input)) return 2;
        } else {
            fprintf(stderr, "unknown option: %s\n", arg);
            usage(argv[0]);
            return 2;
        }
    }

    if (check_only) {
        print_release_info_stdout();
        return 0;
    }

    /* Save for boot command */
    g_voices = voices;
    g_frames = frames;
    g_port = port;
    g_output = output;
    g_input = input;

    prefs = repl_prefs_create(REPL_PREFS_USER, "octetta", "skrepl");
    if (!prefs) {
        fprintf(stderr, "could not open skrepl preferences\n");
        return 1;
    }

    app.repl = repl_create("skrepl", 960, 680);
    if (!app.repl) {
        fprintf(stderr, "could not create the FLTK REPL window\n");
        repl_prefs_destroy(prefs);
        return 1;
    }
    load_appearance_preferences(prefs, app.repl);

    repl_set_prompt(app.repl, "# ");
    repl_register_default_commands(app.repl);
    repl_register_command(app.repl, "quit", cmd_quit, &app);
    repl_register_command(app.repl, "exit", cmd_quit, &app);
    repl_register_command(app.repl, "help", gui_help, &app);
    repl_register_command(app.repl, "boot", cmd_boot, &app);
    repl_register_command(app.repl, "credits", cmd_credits, &app);
    repl_set_fallback_handler(app.repl, bitmap_panel_handler, &app);
    panel_set_command_handler(panel_to_skred, &app);
    panel_set_error_handler(panel_error_to_repl, &app);

    /* Initialize FLTK's cross-thread awake support before Skred can invoke
     * a foreign callback from its control-dispatch thread. */
    foreign_bridge_init();
    foreign_bridge_set_handler(foreign_repl_handler, &app);

    print_release_info_repl(&app);
    repl_printf(app.repl, "frames/callback %u; voices %u; UDP port %d\n",
                frames, voices, port);

    skred_set_audio_device(g_output, g_input);
    if (skred_start(g_frames, g_voices, g_port) != 0) {
        repl_println(app.repl, "Could not start the Skred audio engine.");
        repl_println(app.repl, "Close this window after reviewing the message.");
        repl_run(app.repl);
        save_appearance_preferences(prefs, app.repl);
        repl_destroy(app.repl);
        repl_prefs_destroy(prefs);
        return 1;
    }

    /* Enable Skred pattern control events ("yc1") and hook wait_fd into FLTK event loop */
    skred_command("yc1");
    int control_fd = skred_control_event_wait_fd();
    if (control_fd >= 0) {
        repl_add_fd(control_fd, skred_control_event_cb, &app);
    }

    if (skred_spectrogram_bind() != 0) {
        repl_println(app.repl, "Could not bind the spectrogram data bridge.");
    }

    /* Slot 9 belongs to SpectrogramBridge; reserve slot 8 for GUI strings. */
    if (skred_foreign_function_bind(8, fltk_repl_foreign_call, &app) != 0) {
        repl_println(app.repl, "Could not bind the Skode GUI-string callback.");
    }

    skred_logger(1);
    repl_println(app.repl,
        "Type 'help' for REPL commands.\n"
        "Type '/h' for SKODE commands");

    i = repl_run(app.repl);
    skred_control_dispatch_stop();
    skred_foreign_function_clear(8);
    foreign_bridge_shutdown();
    skred_spectrogram_unbind();
    topology_hide();
    skred_stop();
    panel_set_error_handler(NULL, NULL);
    save_appearance_preferences(prefs, app.repl);
    repl_destroy(app.repl);
    repl_prefs_destroy(prefs);

    return i;
}
