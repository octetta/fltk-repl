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
#include "SpectrogramBridge.h"
#include "TopologyWindow.h"
#include <skred/api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void print_skred_log(app_state *app) {
    char *log = skred_log();
    if (log && *log) repl_print(app->repl, log);
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
    if (result > 0) repl_printf(app->repl, "r = %d\n", result);
    if (result < 0) repl_quit(app->repl);
}

static void bitmap_panel_handler(const char *line, void *userdata) {
    app_state *app = (app_state *)userdata;
    char cmd[64] = {0}, arg[256] = {0};
    int n = sscanf(line, "%63s %255[^\n]", cmd, arg);

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
            "usage: spectrogram wave <slot> | record [-1|0|1]");
        return;
    }

    if (strcmp(cmd, "waveform") == 0) {
        int value;
        char extra;
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
            "usage: waveform wave <slot> | record [-1|0|1]");
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
        static panel_win_t *pw = NULL;
        static char last_path[512] = "controls.pnl";

        if (n >= 2 && strncmp(arg, "load ", 5) == 0) {
            const char *path = arg + 5;
            strncpy(last_path, path, sizeof(last_path)-1);
            last_path[sizeof(last_path)-1] = '\0';
            if (pw) panel_destroy(pw);
            pw = panel_load_file(path);
            if (pw) {
                panel_show(pw);
            } else {
                repl_println(app->repl, "Panel load failed — check stderr for details");
            }
            return;
        }
        if (n >= 2 && strcmp(arg, "reload") == 0) {
            if (pw) {
                if (panel_reload_file(pw, last_path) == 0) {
                    repl_println(app->repl, "Panel reloaded successfully.");
                } else {
                    repl_println(app->repl, "Panel reload failed.");
                }
            }
            return;
        }
        if (n >= 2 && strcmp(arg, "hide") == 0) {
            if (pw) panel_hide(pw);
            return;
        }
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
    (void)argc; (void)argv;
    repl_println(app->repl,
        "GUI commands:\n"
        "  clear                    clear scrollback\n"
        "  theme light|dark         change colors\n"
        "  font \"name\" [size]       change terminal font\n"
        "  bitmap [show|hide|clear] graphics output window\n"
        "  spectrogram wave <slot> | record [-1|0|1]\n"
        "  waveform wave <slot> | record [-1|0|1]\n"
        "  topology <voice> [depth] show /vg voice topology\n"
        "  panel load <file.pnl> | reload | hide\n"
        "  boot [voices N] [frames N] [port N]   restart Skred\n"
        "  credits\n"
        "  quit / exit              stop everything\n"
        "\n"
        "Every other line is sent to Skred.");
}

static void cmd_quit(int argc, char **argv, void *userdata) {
    app_state *app = (app_state *)userdata;
    (void)argc;
    (void)argv;
    skred_spectrogram_unbind();
    topology_hide();
    bitmap_win_hide_all();
    repl_quit(app->repl);
}

static void panel_to_skred(const char *line, void *user_data) {
    (void)user_data;
    size_t len = strlen(line);
    char *cmd = (char *)malloc(len + 1);
    if (cmd) {
        memcpy(cmd, line, len + 1);
        skred_command(cmd);
        free(cmd);
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
    const char *theme = repl_get_theme(repl) == REPL_THEME_LIGHT ? "light" : "dark";

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
    panel_set_command_handler(panel_to_skred, NULL);

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

    if (skred_spectrogram_bind() != 0) {
        repl_println(app.repl, "Could not bind the spectrogram data bridge.");
    }

    skred_logger(1);
    repl_println(app.repl,
        "Type 'help' for REPL commands.\n"
        "Type '/h' for SKODE commands");

    i = repl_run(app.repl);
    skred_spectrogram_unbind();
    topology_hide();
    skred_control_dispatch_stop();
    skred_stop();
    save_appearance_preferences(prefs, app.repl);
    repl_destroy(app.repl);
    repl_prefs_destroy(prefs);

    return i;
}
