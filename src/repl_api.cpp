#include "repl/repl_api.h"
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
#include <vector>

struct CommandEntry {
    repl_cmd_fn fn;
    void *userdata;
};

struct repl_ctx {
    Fl_Double_Window *window = nullptr;
    TerminalView *term = nullptr;
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

static void cmd_help(int, char **, void *ud) {
    repl_ctx *ctx = (repl_ctx *)ud;
    std::string out = "Commands:\n";
    for (auto &kv : ctx->commands) {
        out += "  " + kv.first + "\n";
    }
    repl_print(ctx, out.c_str());
}

static void cmd_clear(int, char **, void *ud) {
    repl_clear((repl_ctx *)ud);
}

static void cmd_theme(int argc, char **argv, void *ud) {
    repl_ctx *ctx = (repl_ctx *)ud;
    if (argc < 2) {
        repl_println(ctx, "usage: theme light|dark");
        return;
    }
    if (strcmp(argv[1], "dark") == 0) {
        repl_set_theme(ctx, REPL_THEME_DARK);
    } else if (strcmp(argv[1], "light") == 0) {
        repl_set_theme(ctx, REPL_THEME_LIGHT);
    } else {
        repl_println(ctx, "usage: theme light|dark");
    }
}

static void cmd_font(int argc, char **argv, void *ud) {
    repl_ctx *ctx = (repl_ctx *)ud;
    if (argc < 2) {
        repl_printf(ctx, "current font: %s %dpt\n", ctx->font_name.c_str(), ctx->font_size);
        repl_println(ctx, "monospace fonts found:");
        char buf[4096];
        repl_list_fonts_filtered(ctx, buf, sizeof(buf), 1, kFontProbeSize);
        repl_print(ctx, buf);
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

    repl_apply_global_scheme(true);

    ctx->window = new Fl_Double_Window(width, height, title ? title : "REPL");
    ctx->window->resizable(ctx->window);

    ctx->term = new TerminalView(0, 0, width, height);
    ctx->term->textfont(FL_COURIER);
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
    delete ctx->window; // deletes child widgets (term) too
    delete ctx;
}

// Global pointer to active terminal view
static TerminalView* g_active_term = nullptr;

static int repl_copy_handler(int event) {
    if (event == FL_KEYBOARD) {
        // Intercept Ctrl+C (Linux/Win) or Cmd+C (macOS)
        if ((Fl::event_state() & (FL_CTRL | FL_COMMAND)) && Fl::event_key() == 'c') {
            if (g_active_term) {
                // Cast TerminalView to Fl_Text_Display to access the buffer
                Fl_Text_Display* disp = static_cast<Fl_Text_Display*>(g_active_term);
                if (disp && disp->buffer()) {
                    Fl_Text_Buffer* buf = disp->buffer();
                    
                    // If text is highlighted, copy directly to CLIPBOARD (1)
                    if (buf->selected()) {
                        char* text = buf->selection_text();
                        if (text) {
                            Fl::copy(text, (int)strlen(text), 1); // 1 = System Clipboard
                            free(text);
                            return 1; // Event consumed
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
    return Fl::run();
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

void repl_register_default_commands(repl_ctx *ctx) {
    if (!ctx) return;
    repl_register_command(ctx, "help", cmd_help, ctx);
    repl_register_command(ctx, "clear", cmd_clear, ctx);
    repl_register_command(ctx, "theme", cmd_theme, ctx);
    repl_register_command(ctx, "font", cmd_font, ctx);
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
    ctx->theme = theme;
    repl_apply_global_scheme(theme == REPL_THEME_DARK);
    ctx->term->setColors(repl_theme_defaults(theme == REPL_THEME_DARK));
    ctx->window->redraw();
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
