#include "Theme.h"
#include <FL/fl_draw.H>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {
static bool g_dark_mode = false;
static bool g_is_custom = false;
static ReplCustomTheme g_custom_theme;

static std::string get_default_theme_file() {
    const char *home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) return ".skrepl_theme.conf";
    std::string dir = std::string(home) + "/.config/skrepl";
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
    return dir + "/theme.conf";
}

static void draw_bootstrap_flat_box(int x, int y, int w, int h, Fl_Color bg, Fl_Color border, int radius) {
    if (w <= 0 || h <= 0) return;
    fl_color(bg);
    if (radius > 0) {
        fl_rounded_rectf(x, y, w, h, radius);
        fl_color(border);
        fl_rounded_rect(x, y, w, h, radius);
    } else {
        fl_rectf(x, y, w, h);
        fl_color(border);
        fl_rect(x, y, w, h);
    }
}

// FL_UP_BOX: Normal buttons & interactive control surfaces
static void draw_bootstrap_up_box(int x, int y, int w, int h, Fl_Color c) {
    Fl_Color bg = (c == FL_BACKGROUND_COLOR)
        ? (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.card)
            : (g_dark_mode ? fl_rgb_color(0x33, 0x41, 0x55) : fl_rgb_color(0xe2, 0xe8, 0xf0)))
        : c;
    Fl_Color border = g_is_custom
        ? repl_rgb_to_flcolor(g_custom_theme.border)
        : (g_dark_mode ? fl_rgb_color(0x47, 0x55, 0x69) : fl_rgb_color(0xcb, 0xd5, 0xe1));
    draw_bootstrap_flat_box(x, y, w, h, bg, border, 4);
}

// FL_DOWN_BOX: Pressed buttons & active state
static void draw_bootstrap_down_box(int x, int y, int w, int h, Fl_Color c) {
    Fl_Color bg = (c == FL_BACKGROUND_COLOR)
        ? (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.accent)
            : (g_dark_mode ? fl_rgb_color(0x0d, 0x6e, 0xfd) : fl_rgb_color(0x0d, 0x6e, 0xfd)))
        : c;
    Fl_Color border = g_is_custom
        ? repl_rgb_to_flcolor(g_custom_theme.focus)
        : (g_dark_mode ? fl_rgb_color(0x38, 0xbd, 0xf8) : fl_rgb_color(0x0b, 0x5e, 0xd7));
    draw_bootstrap_flat_box(x, y, w, h, bg, border, 4);
}

// FL_THIN_UP_BOX: Flat panel containers / card wrappers
static void draw_bootstrap_thin_up_box(int x, int y, int w, int h, Fl_Color c) {
    Fl_Color bg = (c == FL_BACKGROUND_COLOR)
        ? (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.card)
            : (g_dark_mode ? fl_rgb_color(0x1e, 0x29, 0x3b) : fl_rgb_color(0xff, 0xff, 0xff)))
        : c;
    Fl_Color border = g_is_custom
        ? repl_rgb_to_flcolor(g_custom_theme.border)
        : (g_dark_mode ? fl_rgb_color(0x33, 0x41, 0x55) : fl_rgb_color(0xe2, 0xe8, 0xf0));
    draw_bootstrap_flat_box(x, y, w, h, bg, border, 6);
}

static bool check_widget_focus(int x, int y, int w, int h) {
    Fl_Widget *f = Fl::focus();
    if (!f) return false;
    int fx = f->x(), fy = f->y(), fw = f->w(), fh = f->h();
    return (x < fx + fw && x + w > fx && y < fy + fh && y + h > fy);
}

// FL_THIN_DOWN_BOX: Inset input fields, dropdown choices & slider troughs
static void draw_bootstrap_thin_down_box(int x, int y, int w, int h, Fl_Color c) {
    bool has_focus = check_widget_focus(x, y, w, h);
    Fl_Color bg = (c == FL_BACKGROUND2_COLOR || c == FL_BACKGROUND_COLOR)
        ? (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.card)
            : (g_dark_mode ? fl_rgb_color(0x1e, 0x29, 0x3b) : fl_rgb_color(0xe2, 0xe8, 0xf0)))
        : c;
    Fl_Color border = has_focus
        ? (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.focus)
            : (g_dark_mode ? fl_rgb_color(0x38, 0xbd, 0xf8) : fl_rgb_color(0x0d, 0x6e, 0xfd)))
        : (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.border)
            : (g_dark_mode ? fl_rgb_color(0x47, 0x55, 0x69) : fl_rgb_color(0xcb, 0xd5, 0xe1)));
    draw_bootstrap_flat_box(x, y, w, h, bg, border, 4);

    if (has_focus) {
        Fl_Color ring = g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.focus)
            : (g_dark_mode ? fl_rgb_color(0x38, 0xbd, 0xf8) : fl_rgb_color(0x0d, 0x6e, 0xfd));
        fl_color(ring);
        fl_rounded_rect(x + 1, y + 1, w - 2, h - 2, 3);
    }
}

// FL_ROUNDED_BOX / FL_RFLAT_BOX: Cards, badges & pill buttons
static void draw_bootstrap_rounded_box(int x, int y, int w, int h, Fl_Color c) {
    Fl_Color bg = (c == FL_BACKGROUND_COLOR)
        ? (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.card)
            : (g_dark_mode ? fl_rgb_color(0x1e, 0x29, 0x3b) : fl_rgb_color(0xff, 0xff, 0xff)))
        : c;
    Fl_Color border = g_is_custom
        ? repl_rgb_to_flcolor(g_custom_theme.border)
        : (g_dark_mode ? fl_rgb_color(0x33, 0x41, 0x55) : fl_rgb_color(0xe2, 0xe8, 0xf0));
    draw_bootstrap_flat_box(x, y, w, h, bg, border, 6);
}

// FL_SLIDER_CHA: Scrollbar & Slider Channels/Troughs
static void draw_bootstrap_slider_channel(int x, int y, int w, int h, Fl_Color c) {
    Fl_Color bg = (c == FL_BACKGROUND2_COLOR || c == FL_BACKGROUND_COLOR)
        ? (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.card)
            : (g_dark_mode ? fl_rgb_color(0x1e, 0x29, 0x3b) : fl_rgb_color(0xe2, 0xe8, 0xf0)))
        : c;
    Fl_Color border = g_is_custom
        ? repl_rgb_to_flcolor(g_custom_theme.border)
        : (g_dark_mode ? fl_rgb_color(0x33, 0x41, 0x55) : fl_rgb_color(0xcb, 0xd5, 0xe1));
    draw_bootstrap_flat_box(x, y, w, h, bg, border, 3);
}

// FL_SLIDER_BOX: Scrollbar & Slider Knobs/Thumbs
static void draw_bootstrap_slider_box(int x, int y, int w, int h, Fl_Color c) {
    Fl_Color bg = (c == FL_BACKGROUND_COLOR)
        ? (g_is_custom
            ? repl_rgb_to_flcolor(g_custom_theme.accent)
            : (g_dark_mode ? fl_rgb_color(0x38, 0xbd, 0xf8) : fl_rgb_color(0x0d, 0x6e, 0xfd)))
        : c;
    Fl_Color border = g_is_custom
        ? repl_rgb_to_flcolor(g_custom_theme.focus)
        : (g_dark_mode ? fl_rgb_color(0x38, 0xbd, 0xf8) : fl_rgb_color(0x0b, 0x5e, 0xd7));
    draw_bootstrap_flat_box(x, y, w, h, bg, border, 3);
}
} // namespace

unsigned int repl_parse_color(const char *str, unsigned int default_val) {
    if (!str || !*str) return default_val;
    const char *p = str;
    if (*p == '#') p++;
    else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    char *end = NULL;
    unsigned long val = strtoul(p, &end, 16);
    if (end && end != p) return (unsigned int)val;
    return default_val;
}

Fl_Color repl_rgb_to_flcolor(unsigned int rgb) {
    unsigned char r = (unsigned char)((rgb >> 16) & 0xFF);
    unsigned char g = (unsigned char)((rgb >> 8) & 0xFF);
    unsigned char b = (unsigned char)(rgb & 0xFF);
    return fl_rgb_color(r, g, b);
}

ReplColors repl_theme_defaults(bool dark) {
    ReplColors c{};
    if (dark) {
        c.bg     = 0x0f172a; // Slate dark background (#0f172a)
        c.fg     = 0xf8f9fa; // Bootstrap light text (#f8f9fa)
        c.prompt = 0x38bdf8; // Bootstrap cyan accent (#38bdf8)
        c.input  = 0xffffff;
        c.cursor = 0x38bdf8;
    } else {
        c.bg     = 0xf8f9fa; // Bootstrap 5 body background (#f8f9fa)
        c.fg     = 0x212529; // Bootstrap dark body text (#212529)
        c.prompt = 0x0d6efd; // Bootstrap primary blue (#0d6efd)
        c.input  = 0x212529;
        c.cursor = 0x0d6efd;
    }
    return c;
}

void repl_apply_global_scheme(bool dark) {
    g_is_custom = false;
    g_dark_mode = dark;
    Fl::scheme(nullptr);

    if (dark) {
        Fl::background(0x1e, 0x29, 0x3b);                      // #1e293b Slate Card BG
        Fl::background2(0x0f, 0x17, 0x2a);                     // #0f172a Input / Terminal BG
        Fl::foreground(0xf8, 0xf9, 0xfa);                      // #f8f9fa Text
        Fl::set_color(FL_SELECTION_COLOR, 0x1e, 0x40, 0xaf);    // #1e40af Dark Blue Selection
    } else {
        Fl::background(0xf1, 0xf5, 0xf9);                      // #f1f5f9 App / Body Canvas BG
        Fl::background2(0xff, 0xff, 0xff);                     // #ffffff Input / Card BG
        Fl::foreground(0x21, 0x25, 0x29);                      // #212529 Text
        Fl::set_color(FL_SELECTION_COLOR, 0xc7, 0xd2, 0xfe);    // #c7d2fe Soft Sky Blue Selection
    }

    Fl::set_boxtype(FL_UP_BOX, draw_bootstrap_up_box, 1, 1, 2, 2);
    Fl::set_boxtype(FL_DOWN_BOX, draw_bootstrap_down_box, 1, 1, 2, 2);
    Fl::set_boxtype(FL_THIN_UP_BOX, draw_bootstrap_thin_up_box, 1, 1, 2, 2);
    Fl::set_boxtype(FL_THIN_DOWN_BOX, draw_bootstrap_thin_down_box, 1, 1, 2, 2);
    Fl::set_boxtype(FL_ROUNDED_BOX, draw_bootstrap_rounded_box, 2, 2, 4, 4);
    Fl::set_boxtype(FL_RFLAT_BOX, draw_bootstrap_rounded_box, 2, 2, 4, 4);

    Fl::reload_scheme();
}

void repl_apply_custom_theme(const ReplCustomTheme &t) {
    g_is_custom = true;
    g_custom_theme = t;
    g_dark_mode = t.is_dark;

    Fl::scheme(nullptr);
    Fl::background((t.card >> 16) & 0xff, (t.card >> 8) & 0xff, t.card & 0xff);
    Fl::background2((t.bg >> 16) & 0xff, (t.bg >> 8) & 0xff, t.bg & 0xff);
    Fl::foreground((t.fg >> 16) & 0xff, (t.fg >> 8) & 0xff, t.fg & 0xff);
    Fl::set_color(FL_SELECTION_COLOR, (t.focus >> 16) & 0xff, (t.focus >> 8) & 0xff, t.focus & 0xff);

    Fl::set_boxtype(FL_UP_BOX, draw_bootstrap_up_box, 1, 1, 2, 2);
    Fl::set_boxtype(FL_DOWN_BOX, draw_bootstrap_down_box, 1, 1, 2, 2);
    Fl::set_boxtype(FL_THIN_UP_BOX, draw_bootstrap_thin_up_box, 1, 1, 2, 2);
    Fl::set_boxtype(FL_THIN_DOWN_BOX, draw_bootstrap_thin_down_box, 1, 1, 2, 2);
    Fl::set_boxtype(FL_ROUNDED_BOX, draw_bootstrap_rounded_box, 2, 2, 4, 4);
    Fl::set_boxtype(FL_RFLAT_BOX, draw_bootstrap_rounded_box, 2, 2, 4, 4);

    Fl::reload_scheme();
}

ReplCustomTheme repl_get_active_custom_theme() {
    if (g_is_custom) return g_custom_theme;
    ReplCustomTheme t;
    t.is_dark = g_dark_mode;
    if (g_dark_mode) {
        t.bg = 0x0f172a; t.card = 0x1e293b; t.fg = 0xf8f9fa;
        t.accent = 0x0d6efd; t.focus = 0x38bdf8; t.border = 0x334155;
    } else {
        t.bg = 0xf1f5f9; t.card = 0xffffff; t.fg = 0x212529;
        t.accent = 0x0d6efd; t.focus = 0x0d6efd; t.border = 0xcbd5e1;
    }
    return t;
}

bool repl_load_theme_file(const char *filename, ReplCustomTheme &out) {
    std::string path = (filename && *filename) ? filename : get_default_theme_file();
    std::ifstream in(path.c_str());
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        // Trim leading space
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        line = line.substr(first);
        if (line[0] == '#' || line[0] == ';') continue; // Comment line

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        while (!key.empty() && std::isspace((unsigned char)key.front())) key.erase(0, 1);
        while (!key.empty() && std::isspace((unsigned char)key.back())) key.pop_back();
        while (!val.empty() && std::isspace((unsigned char)val.front())) val.erase(0, 1);
        while (!val.empty() && std::isspace((unsigned char)val.back())) val.pop_back();

        if (key == "bg") out.bg = repl_parse_color(val.c_str(), out.bg);
        else if (key == "card") out.card = repl_parse_color(val.c_str(), out.card);
        else if (key == "fg") out.fg = repl_parse_color(val.c_str(), out.fg);
        else if (key == "accent") out.accent = repl_parse_color(val.c_str(), out.accent);
        else if (key == "focus") out.focus = repl_parse_color(val.c_str(), out.focus);
        else if (key == "border") out.border = repl_parse_color(val.c_str(), out.border);
        else if (key == "is_dark") out.is_dark = (val == "1" || val == "true");
    }
    return true;
}

bool repl_save_theme_file(const char *filename, const ReplCustomTheme &t) {
    std::string path = (filename && *filename) ? filename : get_default_theme_file();
    std::ofstream out(path.c_str());
    if (!out.is_open()) return false;

    out << "# Skred REPL Custom Theme Configuration\n";
    char hex[32];
    snprintf(hex, sizeof hex, "bg = #%06x\n", t.bg & 0xffffff); out << hex;
    snprintf(hex, sizeof hex, "card = #%06x\n", t.card & 0xffffff); out << hex;
    snprintf(hex, sizeof hex, "fg = #%06x\n", t.fg & 0xffffff); out << hex;
    snprintf(hex, sizeof hex, "accent = #%06x\n", t.accent & 0xffffff); out << hex;
    snprintf(hex, sizeof hex, "focus = #%06x\n", t.focus & 0xffffff); out << hex;
    snprintf(hex, sizeof hex, "border = #%06x\n", t.border & 0xffffff); out << hex;
    out << "is_dark = " << (t.is_dark ? "true" : "false") << "\n";
    return true;
}
