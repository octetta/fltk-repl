#include "repl/repl_api.h"
#include "repl/editor_win.h"
#include "repl/udp_bridge.h"
#include "repl/font_picker.h"
#include "EditorWindow.h"
#include "FontPickerWindow.h"
#include "UdpBridge.h"
#include "TerminalView.h"
#include "Theme.h"
#include "Tokenize.h"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>

#include <FL/fl_draw.H>   // add near the other FL/ includes, for fl_font/fl_width

#include <FL/Fl_Text_Display.H>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <cmath>
#include <string>
#include <sstream>
#include <vector>

struct CommandEntry {
    repl_cmd_fn fn;
    void *userdata;
};

struct repl_ctx {
    Fl_Double_Window *window = nullptr;
    TerminalView *term = nullptr;
    UdpBridge *udp = nullptr;
    std::map<std::string, CommandEntry> commands;
    repl_line_fn fallback_fn = nullptr;
    void *fallback_userdata = nullptr;
    repl_theme theme = REPL_THEME_DARK;
    std::string font_name = "Courier";
    int font_size = 14;
};

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

static const int kFontProbeSize = 14;  // reference size for monospace detection,
                                        // independent of the terminal's current font_size

static char *dup_cstr(const std::string &s) {
    char *p = (char *)malloc(s.size() + 1);
    if (!p) return nullptr;
    memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

static void dispatch_line(repl_ctx *ctx, const std::string &line) {
    std::vector<std::string> tokens = repl_tokenize(line);
    if (tokens.empty()) {
        ctx->term->showPrompt();
        return;
    }

    auto it = ctx->commands.find(tokens[0]);
    if (it == ctx->commands.end()) {
        if (ctx->udp && ctx->udp->isConnected() && ctx->udp->mode() == UdpBridge::MODE_FORWARD) {
            ctx->udp->sendData(line.c_str(), line.size());
            ctx->term->showPrompt();
            return;
        }
        if (ctx->fallback_fn) {
            ctx->fallback_fn(line.c_str(), ctx->fallback_userdata);
            ctx->term->showPrompt();
            return;
        }
        std::string msg = "unknown command: " + tokens[0] +
                           " (type 'help' for a list)\n";
        ctx->term->appendOutput(msg);
        ctx->term->showPrompt();
        return;
    }

    std::vector<char *> argv;
    argv.reserve(tokens.size());
    for (auto &t : tokens) argv.push_back(const_cast<char *>(t.c_str()));

    CommandEntry entry = it->second; // copy: fn may register/unregister commands
    entry.fn((int)argv.size(), argv.data(), entry.userdata);

    ctx->term->showPrompt();
}

// Hand-rolled case-insensitive compare/contains so we don't depend on
// strcasecmp/strcasestr, which aren't portable to MSVC/Windows.
static bool ci_equal(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
}

static bool ci_contains(const std::string &haystack, const std::string &needle) {
    if (needle.empty()) return true;
    std::string h = haystack, n = needle;
    for (auto &c : h) c = (char)tolower((unsigned char)c);
    for (auto &c : n) c = (char)tolower((unsigned char)c);
    return h.find(n) != std::string::npos;
}

static Fl_Font find_font_by_name(const char *name) {
    int n = Fl::set_fonts("-*");
    for (int i = 0; i < n; ++i) {
        int attr = 0;
        const char *fname = Fl::get_font_name((Fl_Font)i, &attr);
        if (fname && ci_equal(fname, name)) {
            return (Fl_Font)i;
        }
    }
    // Fall back to substring match, e.g. "DejaVu" matching "DejaVu Sans Mono".
    for (int i = 0; i < n; ++i) {
        int attr = 0;
        const char *fname = Fl::get_font_name((Fl_Font)i, &attr);
        if (fname && ci_contains(fname, name)) {
            return (Fl_Font)i;
        }
    }
    return (Fl_Font)-1;
}

static Fl_Font find_default_terminal_font(std::string &matched_name) {
    // Prefer monospace faces known to include terminal drawing characters,
    // especially the Braille Patterns block used by Skred scopes. The list is
    // ordered per platform, then falls back harmlessly on machines where none
    // of the preferred faces is installed.
#ifdef _WIN32
    const char *candidates[] = {
        "Cascadia Mono", "Cascadia Code", "Consolas", "DejaVu Sans Mono"
    };
#elif defined(__APPLE__)
    const char *candidates[] = {
        "Menlo", "SF Mono", "DejaVu Sans Mono", "Noto Sans Mono"
    };
#else
    const char *candidates[] = {
        "Adwaita Mono", "GNU Unifont", "Unifont", "DejaVu Sans Mono",
        "Noto Sans Mono"
    };
#endif

    for (const char *candidate : candidates) {
        Fl_Font font = find_font_by_name(candidate);
        if (font >= 0) {
            int attr = 0;
            const char *name = Fl::get_font_name(font, &attr);
            matched_name = name ? name : candidate;
            return font;
        }
    }

    matched_name = "Courier";
    return FL_COURIER;
}

static bool font_looks_monospace(Fl_Font f, int size) {
    fl_font(f, size);
    double wi = fl_width("i");
    double wM = fl_width("M");
    double wl = fl_width("l");
    return wi > 0.0 && fabs(wi - wM) < 0.01 && fabs(wi - wl) < 0.01;
}

// ---------------------------------------------------------------------
// default builtin commands
// ---------------------------------------------------------------------

static void cmd_help(int argc, char **argv, void *ud) {
    repl_ctx *ctx = (repl_ctx *)ud;
    if (!ctx) return;

    if (argc >= 2) {
        std::string topic = argv[1];
        if (topic == "edit" || topic == "editor") {
            repl_println(ctx,
                "Usage: edit [filepath]\n"
                "  Opens the built-in desktop script text editor window.\n"
                "  Features line numbers, Open/Save file dialogs, dark/light themes, and\n"
                "  one-click or Ctrl+Enter / Cmd+Enter execution into the REPL session.");
            return;
        }
        if (topic == "udp") {
            repl_println(ctx,
                "Usage: udp connect <host> <port> | send <text> | mode forward|log|off | color <1..255> | status | disconnect\n"
                "  Attach to external programs or remote instances via UDP.\n"
                "  Incoming UDP responses are displayed in custom ANSI 256 color (default 51 = cyan).");
            return;
        }
    }

    std::string out = "Commands:\n";
    for (auto &kv : ctx->commands) {
        out += "  " + kv.first;
        if (kv.first == "help") out += "                            - list available commands";
        else if (kv.first == "clear") out += "                           - clear terminal scrollback";
        else if (kv.first == "theme") out += "                           - switch or save color themes";
        else if (kv.first == "font") out += "                            - view or change terminal font";
        else if (kv.first == "edit" || kv.first == "editor") out += "                     - open script text editor window";
        else if (kv.first == "udp") out += "                             - attach/send via UDP socket";
        else if (kv.first == "quit" || kv.first == "exit") out += "                        - close session";
        out += "\n";
    }
    repl_print(ctx, out.c_str());
}

static void cmd_clear(int, char **, void *ud) {
    repl_clear((repl_ctx *)ud);
}

static void cmd_theme(int argc, char **argv, void *ud) {
    repl_ctx *ctx = (repl_ctx *)ud;
    if (argc < 2) {
        repl_theme cur = repl_get_theme(ctx);
        const char *tname = (cur == REPL_THEME_LIGHT) ? "light" : ((cur == REPL_THEME_DARK) ? "dark" : "custom");
        repl_printf(ctx, "current theme: %s\n", tname);
        repl_println(ctx, "usage:");
        repl_println(ctx, "  theme light | dark | custom");
        repl_println(ctx, "  theme custom <config_file.conf>");
        repl_println(ctx, "  theme custom <bg> <card> <fg> <accent> <focus> [border]");
        repl_println(ctx, "  theme save [config_file.conf]");
        return;
    }

    if (strcmp(argv[1], "dark") == 0) {
        repl_set_theme(ctx, REPL_THEME_DARK);
        repl_println(ctx, "theme set to dark");
    } else if (strcmp(argv[1], "light") == 0) {
        repl_set_theme(ctx, REPL_THEME_LIGHT);
        repl_println(ctx, "theme set to light");
    } else if (strcmp(argv[1], "custom") == 0) {
        if (argc >= 6) {
            // theme custom #0f172a #1e293b #f8f9fa #0d6efd #38bdf8 [#475569]
            unsigned int bg     = repl_parse_color(argv[2], 0xf1f5f9);
            unsigned int card   = repl_parse_color(argv[3], 0xffffff);
            unsigned int fg     = repl_parse_color(argv[4], 0x212529);
            unsigned int accent = repl_parse_color(argv[5], 0x0d6efd);
            unsigned int focus  = (argc >= 7) ? repl_parse_color(argv[6], accent) : accent;
            unsigned int border = (argc >= 8) ? repl_parse_color(argv[7], 0xcbd5e1) : 0xcbd5e1;
            int is_dark = ((bg & 0xff) + ((bg >> 8) & 0xff) + ((bg >> 16) & 0xff) < 384) ? 1 : 0;

            repl_set_custom_theme(ctx, bg, card, fg, accent, focus, border, is_dark);
            repl_save_theme_file(ctx, NULL);
            repl_println(ctx, "custom theme updated and saved to ~/.config/skrepl/theme.conf");
        } else if (argc == 3) {
            // theme custom <filepath.conf>
            if (repl_load_theme_file(ctx, argv[2])) {
                repl_printf(ctx, "loaded custom theme from %s\n", argv[2]);
            } else {
                repl_printf(ctx, "failed to load theme file: %s\n", argv[2]);
            }
        } else {
            // theme custom (load default config file or defaults)
            if (!repl_load_theme_file(ctx, NULL)) {
                ReplCustomTheme t;
                repl_apply_custom_theme(t);
                ctx->theme = REPL_THEME_CUSTOM;
                repl_save_theme_file(ctx, NULL);
            }
            repl_println(ctx, "custom theme active (from ~/.config/skrepl/theme.conf)");
        }
    } else if (strcmp(argv[1], "save") == 0) {
        const char *filepath = (argc >= 3) ? argv[2] : NULL;
        if (repl_save_theme_file(ctx, filepath)) {
            repl_printf(ctx, "theme saved to %s\n", filepath ? filepath : "~/.config/skrepl/theme.conf");
        } else {
            repl_println(ctx, "failed to save theme file");
        }
    } else {
        repl_println(ctx, "usage: theme light|dark|custom|save");
    }
}

static void cmd_font(int argc, char **argv, void *ud) {
    repl_ctx *ctx = (repl_ctx *)ud;
    if (!ctx) return;

    if (argc < 2 || strcmp(argv[1], "choose") == 0 || strcmp(argv[1], "picker") == 0 || strcmp(argv[1], "gui") == 0) {
        FontPickerWindow *win = font_picker_get_or_create();
        win->setColors(ctx->term->colors());
        win->setInitialFont(ctx->font_name, ctx->font_size);
        win->setApplyHandler([ctx](const std::string &fontName, int fontSize) {
            repl_set_font(ctx, fontName.c_str(), fontSize);
        });
        win->show();
        return;
    }

    int size = ctx->font_size;
    if (argc >= 3) size = atoi(argv[2]);
    if (!repl_set_font(ctx, argv[1], size)) {
        repl_printf(ctx, "font not found: %s\n", argv[1]);
    }
}

static void cmd_quit(int, char **, void *ud) {
    repl_quit((repl_ctx *)ud);
}

// ---------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------

repl_ctx *repl_create(const char *title, int width, int height) {
    repl_ctx *ctx = new repl_ctx();
    ctx->udp = new UdpBridge(ctx);

    repl_apply_global_scheme(true);

    ctx->window = new Fl_Double_Window(width, height, title ? title : "REPL");
    ctx->window->resizable(ctx->window);

    ctx->term = new TerminalView(0, 0, width, height);
    Fl_Font default_font = find_default_terminal_font(ctx->font_name);
    ctx->term->setFont(default_font, ctx->font_size);
    ctx->term->selection_color(fl_rgb_color(60, 120, 180)); // High-contrast blue highlight
    ctx->term->setColors(repl_theme_defaults(true));
    ctx->term->setLineHandler([ctx](const std::string &line) {
        dispatch_line(ctx, line);
    });

    ctx->window->end();
    ctx->window->resizable(ctx->term);
    ctx->window->show();

    ctx->term->take_focus();

    return ctx;
}

void repl_destroy(repl_ctx *ctx) {
    if (!ctx) return;
    delete ctx->udp;
    delete ctx->window; // deletes child widgets (term) too
    delete ctx;
}

// Global pointer to active terminal view
static TerminalView* g_active_term = nullptr;

static int repl_copy_handler(int event) {
    if (event == FL_KEYBOARD) {
        // Intercept Ctrl+C (Linux/Win) or Cmd+C (macOS)
        if ((Fl::event_state() & (FL_CTRL | FL_COMMAND)) && (Fl::event_key() == 'c' || Fl::event_key() == 'C')) {
            if (g_active_term) {
                Fl_Text_Display* disp = static_cast<Fl_Text_Display*>(g_active_term);
                if (disp && disp->buffer()) {
                    Fl_Text_Buffer* buf = disp->buffer();
                    
                    if (buf->selected()) {
                        char* text = buf->selection_text();
                        if (text) {
                            Fl::copy(text, (int)strlen(text), 1); // 1 = Clipboard
                            Fl::copy(text, (int)strlen(text), 0); // 0 = Primary selection (X11)
                            free(text);
                            return 1; // Event consumed successfully
                        }
                    }
                }
            }
        }
    }
    return 0; // Pass through all other events
}

int repl_run(repl_ctx *ctx) {
    if (!ctx) return -1;

    // Assign active terminal view and register Ctrl+C clipboard handler
    g_active_term = ctx->term;
    Fl::add_handler(repl_copy_handler);

    ctx->term->showPrompt();

    // The initial prompt is added before the event loop has laid out the text
    // display. Reassert focus/cursor on the first loop iteration so FLTK can
    // calculate and paint the caret at an otherwise empty input position.
    Fl::add_timeout(0.0, [](void *userdata) {
        TerminalView *term = static_cast<TerminalView *>(userdata);
        term->take_focus();
        term->show_cursor(1);
        term->redraw();
    }, ctx->term);
    return Fl::run();
}

void repl_dispatch_line(repl_ctx *ctx, const char *line) {
    if (!ctx || !line) return;
    dispatch_line(ctx, line);
}

void repl_quit(repl_ctx *ctx) {
    if (!ctx || !ctx->window) return;
    ctx->window->hide();
}

void repl_register_command(repl_ctx *ctx, const char *name, repl_cmd_fn fn, void *userdata) {
    if (!ctx || !name || !fn) return;
    ctx->commands[name] = CommandEntry{fn, userdata};
}

void repl_unregister_command(repl_ctx *ctx, const char *name) {
    if (!ctx || !name) return;
    ctx->commands.erase(name);
}

void repl_set_fallback_handler(repl_ctx *ctx, repl_line_fn fn, void *userdata) {
    if (!ctx) return;
    ctx->fallback_fn = fn;
    ctx->fallback_userdata = userdata;
}

static void cmd_edit(int argc, char **argv, void *ud) {
    repl_ctx *ctx = (repl_ctx *)ud;
    EditorWindow *win = editor_win_get_or_create();
    if (argc >= 2) {
        win->openFile(argv[1]);
    }
    if (ctx) {
        win->setColors(ctx->term->colors());
        win->setEvalHandler([ctx](const std::string &code) {
            std::stringstream ss(code);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty()) {
                    repl_dispatch_line(ctx, line.c_str());
                }
            }
        });
    }
    win->show();
}

static void cmd_udp(int argc, char **argv, void *ud) {
    repl_ctx *ctx = (repl_ctx *)ud;
    if (!ctx || !ctx->udp) return;

    if (argc < 2) {
        repl_println(ctx, "usage: udp connect <host> <port> | send <text> | mode forward|log|off | color <1..255> | status | disconnect");
        return;
    }

    if (strcmp(argv[1], "connect") == 0 || strcmp(argv[1], "attach") == 0) {
        if (argc < 4) {
            repl_println(ctx, "usage: udp connect <host> <port>");
            return;
        }
        const char *host = argv[2];
        int port = atoi(argv[3]);
        if (ctx->udp->connectTarget(host, port)) {
            repl_printf(ctx, "\x1b[38;5;%dm[UDP] Attached to %s:%d (bound local port %d)\x1b[0m\n",
                        ctx->udp->color(), host, port, ctx->udp->localPort());
        } else {
            repl_printf(ctx, "failed to connect to UDP target %s:%d\n", host, port);
        }
    } else if (strcmp(argv[1], "send") == 0) {
        if (argc < 3) {
            repl_println(ctx, "usage: udp send <text>");
            return;
        }
        if (!ctx->udp->isConnected()) {
            repl_println(ctx, "udp: not connected to any target. Use 'udp connect <host> <port>' first.");
            return;
        }
        std::string payload;
        for (int i = 2; i < argc; ++i) {
            payload += argv[i];
            if (i + 1 < argc) payload += " ";
        }
        if (ctx->udp->sendData(payload.c_str(), payload.size())) {
            repl_printf(ctx, "\x1b[38;5;242m[udp sent %zu bytes]\x1b[0m\n", payload.size());
        } else {
            repl_println(ctx, "udp: failed to send packet");
        }
    } else if (strcmp(argv[1], "color") == 0) {
        if (argc < 3) {
            repl_printf(ctx, "current UDP response ANSI color: %d\n", ctx->udp->color());
            return;
        }
        int col = atoi(argv[2]);
        if (col < 1) col = 1;
        if (col > 255) col = 255;
        ctx->udp->setColor(col);
        repl_printf(ctx, "\x1b[38;5;%dmUDP response color set to ANSI code %d\x1b[0m\n", col, col);
    } else if (strcmp(argv[1], "mode") == 0) {
        if (argc < 3) {
            const char *m = (ctx->udp->mode() == UdpBridge::MODE_FORWARD) ? "forward" :
                            (ctx->udp->mode() == UdpBridge::MODE_LOG) ? "log" : "off";
            repl_printf(ctx, "current UDP mode: %s\n", m);
            return;
        }
        if (strcmp(argv[2], "forward") == 0) ctx->udp->setMode(UdpBridge::MODE_FORWARD);
        else if (strcmp(argv[2], "log") == 0) ctx->udp->setMode(UdpBridge::MODE_LOG);
        else if (strcmp(argv[2], "off") == 0) ctx->udp->setMode(UdpBridge::MODE_OFF);
        else {
            repl_println(ctx, "usage: udp mode forward|log|off");
            return;
        }
        repl_printf(ctx, "UDP mode set to %s\n", argv[2]);
    } else if (strcmp(argv[1], "status") == 0) {
        if (ctx->udp->isConnected()) {
            const char *m = (ctx->udp->mode() == UdpBridge::MODE_FORWARD) ? "forward" :
                            (ctx->udp->mode() == UdpBridge::MODE_LOG) ? "log" : "off";
            repl_printf(ctx, "UDP target: \x1b[38;5;%dm%s:%d\x1b[0m (local port %d, mode %s, color %d, sent %llu, recv %llu)\n",
                        ctx->udp->color(), ctx->udp->targetHost().c_str(), ctx->udp->targetPort(),
                        ctx->udp->localPort(), m, ctx->udp->color(),
                        (unsigned long long)ctx->udp->packetsSent(),
                        (unsigned long long)ctx->udp->packetsRecv());
        } else {
            repl_println(ctx, "UDP status: disconnected");
        }
    } else if (strcmp(argv[1], "disconnect") == 0) {
        ctx->udp->disconnectTarget();
        repl_println(ctx, "UDP disconnected.");
    } else {
        repl_println(ctx, "usage: udp connect <host> <port> | send <text> | mode forward|log|off | color <1..255> | status | disconnect");
    }
}

void repl_register_default_commands(repl_ctx *ctx) {
    if (!ctx) return;
    repl_register_command(ctx, "help", cmd_help, ctx);
    repl_register_command(ctx, "clear", cmd_clear, ctx);
    repl_register_command(ctx, "theme", cmd_theme, ctx);
    repl_register_command(ctx, "font", cmd_font, ctx);
    repl_register_command(ctx, "edit", cmd_edit, ctx);
    repl_register_command(ctx, "editor", cmd_edit, ctx);
    repl_register_command(ctx, "udp", cmd_udp, ctx);
    repl_register_command(ctx, "quit", cmd_quit, ctx);
    repl_register_command(ctx, "exit", cmd_quit, ctx);
}

void repl_print(repl_ctx *ctx, const char *utf8_text) {
    if (!ctx || !utf8_text) return;
    ctx->term->appendOutput(utf8_text);
}

void repl_println(repl_ctx *ctx, const char *utf8_text) {
    if (!ctx) return;
    std::string s = utf8_text ? utf8_text : "";
    s += "\n";
    ctx->term->appendOutput(s);
}

void repl_printf(repl_ctx *ctx, const char *fmt, ...) {
    if (!ctx || !fmt) return;
    char stackbuf[1024];
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(stackbuf, sizeof(stackbuf), fmt, ap);
    va_end(ap);

    if (needed < 0) return;

    if ((size_t)needed < sizeof(stackbuf)) {
        ctx->term->appendOutput(stackbuf);
        return;
    }

    std::vector<char> big((size_t)needed + 1);
    va_start(ap, fmt);
    vsnprintf(big.data(), big.size(), fmt, ap);
    va_end(ap);
    ctx->term->appendOutput(big.data());
}

void repl_clear(repl_ctx *ctx) {
    if (!ctx) return;
    ctx->term->clearAll();
    ctx->term->showPrompt();
}

void repl_set_prompt(repl_ctx *ctx, const char *prompt) {
    if (!ctx || !prompt) return;
    ctx->term->setPrompt(prompt);
}

int repl_history_count(repl_ctx *ctx) {
    if (!ctx) return 0;
    return ctx->term->historyCount();
}

const char *repl_history_get(repl_ctx *ctx, int index_from_oldest) {
    if (!ctx) return nullptr;
    return ctx->term->historyAt(index_from_oldest).c_str();
}

void repl_history_clear(repl_ctx *ctx) {
    if (!ctx) return;
    ctx->term->clearHistory();
}

void repl_set_theme(repl_ctx *ctx, repl_theme theme) {
    if (!ctx) return;
    if (theme == REPL_THEME_CUSTOM) {
        ReplCustomTheme t = repl_get_active_custom_theme();
        repl_set_custom_theme(ctx, t.bg, t.card, t.fg, t.accent, t.focus, t.border, t.is_dark);
        return;
    }
    ctx->theme = theme;
    repl_apply_global_scheme(theme == REPL_THEME_DARK);
    ctx->term->setColors(repl_theme_defaults(theme == REPL_THEME_DARK));
    
    // Adjust selection highlight dynamic to theme
    if (theme == REPL_THEME_DARK) {
        ctx->term->selection_color(fl_rgb_color(60, 120, 180));
    } else {
        ctx->term->selection_color(fl_rgb_color(180, 210, 240));
    }
    
    ctx->window->redraw();
}

int repl_set_custom_theme(repl_ctx *ctx,
                           unsigned int bg_rgb,
                           unsigned int card_rgb,
                           unsigned int fg_rgb,
                           unsigned int accent_rgb,
                           unsigned int focus_rgb,
                           unsigned int border_rgb,
                           int is_dark) {
    if (!ctx) return 0;
    ReplCustomTheme t;
    t.bg = bg_rgb;
    t.card = card_rgb;
    t.fg = fg_rgb;
    t.accent = accent_rgb;
    t.focus = focus_rgb;
    t.border = border_rgb;
    t.is_dark = (is_dark != 0);

    ctx->theme = REPL_THEME_CUSTOM;
    repl_apply_custom_theme(t);

    ReplColors rc;
    rc.bg = bg_rgb;
    rc.fg = fg_rgb;
    rc.prompt = accent_rgb;
    rc.input = fg_rgb;
    rc.cursor = focus_rgb;
    ctx->term->setColors(rc);
    ctx->term->selection_color(repl_rgb_to_flcolor(focus_rgb));

    ctx->window->redraw();
    return 1;
}

int repl_load_theme_file(repl_ctx *ctx, const char *filepath) {
    if (!ctx) return 0;
    ReplCustomTheme t = repl_get_active_custom_theme();
    if (!repl_load_theme_file(filepath, t)) return 0;
    return repl_set_custom_theme(ctx, t.bg, t.card, t.fg, t.accent, t.focus, t.border, t.is_dark);
}

int repl_save_theme_file(repl_ctx *ctx, const char *filepath) {
    if (!ctx) return 0;
    ReplCustomTheme t = repl_get_active_custom_theme();
    return repl_save_theme_file(filepath, t) ? 1 : 0;
}

repl_theme repl_get_theme(repl_ctx *ctx) {
    return ctx ? ctx->theme : REPL_THEME_DARK;
}

void repl_set_colors(repl_ctx *ctx, unsigned int bg_rgb, unsigned int fg_rgb,
                      unsigned int prompt_rgb, unsigned int input_rgb) {
    if (!ctx) return;
    ReplColors c = ctx->term->colors();
    c.bg = bg_rgb;
    c.fg = fg_rgb;
    c.prompt = prompt_rgb;
    c.input = input_rgb;
    ctx->term->setColors(c);
}

int repl_set_font(repl_ctx *ctx, const char *font_name, int size) {
    if (!ctx || !font_name) return 0;
    Fl_Font f = find_font_by_name(font_name);
    if (f < 0) return 0;
    ctx->font_name = font_name;
    ctx->font_size = size;
    ctx->term->setFont(f, size);
    return 1;
}

void repl_set_font_size(repl_ctx *ctx, int size) {
    if (!ctx || size <= 0) return;
    ctx->font_size = size;
    Fl_Font f = find_font_by_name(ctx->font_name.c_str());
    if (f < 0) f = FL_COURIER;
    ctx->term->setFont(f, size);
}

const char *repl_get_font_name(repl_ctx *ctx) {
    return ctx ? ctx->font_name.c_str() : nullptr;
}

int repl_get_font_size(repl_ctx *ctx) {
    return ctx ? ctx->font_size : 0;
}

int repl_list_fonts_filtered(repl_ctx *, char *buf, int buf_capacity,
                              int monospace_only, int size) {
    if (!buf || buf_capacity <= 0) return 0;
    buf[0] = '\0';
    int n = Fl::set_fonts("-*");
    int used = 0;
    int found = 0;
    for (int i = 0; i < n; ++i) {
        int attr = 0;
        const char *fname = Fl::get_font_name((Fl_Font)i, &attr);
        if (!fname) continue;
        if (monospace_only && !font_looks_monospace((Fl_Font)i, size)) continue;
        ++found;
        int len = (int)strlen(fname);
        if (used + len + 1 >= buf_capacity) continue; // keep counting, stop writing
        memcpy(buf + used, fname, (size_t)len);
        used += len;
        buf[used++] = '\n';
        buf[used] = '\0';
    }
    return found;
}

int repl_list_fonts(repl_ctx *ctx, char *buf, int buf_capacity) {
    return repl_list_fonts_filtered(ctx, buf, buf_capacity, 0, kFontProbeSize);
}

char *repl_open_file_dialog(repl_ctx *, const char *title, const char *filter) {
    Fl_Native_File_Chooser chooser;
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.title(title ? title : "Open File");
    if (filter) chooser.filter(filter);
    if (chooser.show() == 0) {
        const char *f = chooser.filename();
        if (f && *f) return dup_cstr(f);
    }
    return nullptr;
}

char *repl_save_file_dialog(repl_ctx *, const char *title, const char *filter) {
    Fl_Native_File_Chooser chooser;
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.title(title ? title : "Save File");
    if (filter) chooser.filter(filter);
    if (chooser.show() == 0) {
        const char *f = chooser.filename();
        if (f && *f) return dup_cstr(f);
    }
    return nullptr;
}

char *repl_choose_directory_dialog(repl_ctx *, const char *title) {
    Fl_Native_File_Chooser chooser;
    chooser.type(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
    chooser.title(title ? title : "Choose Directory");
    if (chooser.show() == 0) {
        const char *f = chooser.filename();
        if (f && *f) return dup_cstr(f);
    }
    return nullptr;
}

void repl_free_string(char *s) {
    free(s);
}

void repl_add_fd(int fd, repl_fd_fn cb, void *userdata) {
    if (fd < 0 || !cb) return;
    /* MSVC rejects converting repl_fd_fn (int) to Fl_FD_Handler (FL_SOCKET).
       On Windows FL_SOCKET is SOCKET; values that fit in int are fine for our use. */
    Fl::add_fd(fd, FL_READ, reinterpret_cast<Fl_FD_Handler>(cb), userdata);
}

void repl_remove_fd(int fd) {
    if (fd < 0) return;
    Fl::remove_fd(fd, FL_READ);
}

// ---------------------------------------------------------------------
// editor_win C API
// ---------------------------------------------------------------------
editor_win *repl_editor_open(repl_ctx *ctx) {
    EditorWindow *win = editor_win_get_or_create();
    if (ctx) {
        win->setColors(ctx->term->colors());
        win->setEvalHandler([ctx](const std::string &code) {
            std::stringstream ss(code);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty()) {
                    repl_dispatch_line(ctx, line.c_str());
                }
            }
        });
    }
    win->show();
    return (editor_win *)win;
}

editor_win *repl_editor_open_file(repl_ctx *ctx, const char *filepath) {
    editor_win *win = repl_editor_open(ctx);
    if (filepath && win) {
        ((EditorWindow *)win)->openFile(filepath);
    }
    return win;
}

void repl_editor_close(editor_win *win) {
    if (win) {
        ((EditorWindow *)win)->hide();
    }
}

void repl_editor_set_eval_handler(editor_win *win, editor_eval_fn fn, void *userdata) {
    if (!win) return;
    if (fn) {
        ((EditorWindow *)win)->setEvalHandler([fn, userdata](const std::string &code) {
            fn(code.c_str(), userdata);
        });
    } else {
        ((EditorWindow *)win)->setEvalHandler(nullptr);
    }
}

// ---------------------------------------------------------------------
// udp_bridge C API
// ---------------------------------------------------------------------
int repl_udp_connect(repl_ctx *ctx, const char *host, int port) {
    if (!ctx || !ctx->udp || !host) return 0;
    return ctx->udp->connectTarget(host, port) ? 1 : 0;
}

void repl_udp_disconnect(repl_ctx *ctx) {
    if (ctx && ctx->udp) ctx->udp->disconnectTarget();
}

int repl_udp_send(repl_ctx *ctx, const char *data, size_t len) {
    if (!ctx || !ctx->udp) return -1;
    return ctx->udp->sendData(data, len) ? (int)len : -1;
}

void repl_udp_set_color(repl_ctx *ctx, int ansi_color_code) {
    if (ctx && ctx->udp) ctx->udp->setColor(ansi_color_code);
}

void repl_udp_set_mode(repl_ctx *ctx, const char *mode) {
    if (!ctx || !ctx->udp || !mode) return;
    if (strcmp(mode, "forward") == 0) ctx->udp->setMode(UdpBridge::MODE_FORWARD);
    else if (strcmp(mode, "log") == 0) ctx->udp->setMode(UdpBridge::MODE_LOG);
    else if (strcmp(mode, "off") == 0) ctx->udp->setMode(UdpBridge::MODE_OFF);
}

int repl_udp_is_connected(repl_ctx *ctx) {
    return (ctx && ctx->udp && ctx->udp->isConnected()) ? 1 : 0;
}

// ---------------------------------------------------------------------
// font_picker C API
// ---------------------------------------------------------------------
font_picker_win *repl_font_picker_open(repl_ctx *ctx) {
    FontPickerWindow *win = font_picker_get_or_create();
    if (ctx) {
        win->setColors(ctx->term->colors());
        win->setInitialFont(ctx->font_name, ctx->font_size);
        win->setApplyHandler([ctx](const std::string &fontName, int fontSize) {
            repl_set_font(ctx, fontName.c_str(), fontSize);
        });
    }
    win->show();
    return (font_picker_win *)win;
}

void repl_font_picker_close(font_picker_win *win) {
    if (win) {
        ((FontPickerWindow *)win)->hide();
    }
}

void repl_font_picker_set_handler(font_picker_win *win, font_picker_apply_fn fn, void *userdata) {
    if (!win) return;
    if (fn) {
        ((FontPickerWindow *)win)->setApplyHandler([fn, userdata](const std::string &fontName, int fontSize) {
            fn(fontName.c_str(), fontSize, userdata);
        });
    } else {
        ((FontPickerWindow *)win)->setApplyHandler(nullptr);
    }
}
