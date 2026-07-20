#include "Theme.h"

Fl_Color repl_rgb_to_flcolor(unsigned int rgb) {
    unsigned char r = (unsigned char)((rgb >> 16) & 0xFF);
    unsigned char g = (unsigned char)((rgb >> 8) & 0xFF);
    unsigned char b = (unsigned char)(rgb & 0xFF);
    return fl_rgb_color(r, g, b);
}

ReplColors repl_theme_defaults(bool dark) {
    ReplColors c{};
    if (dark) {
        c.bg     = 0x1e1e1e;
        c.fg     = 0xd4d4d4;
        c.prompt = 0x4ec9b0; // teal
        c.input  = 0xffffff;
        c.cursor = 0xffffff;
    } else {
        c.bg     = 0xfbfbfb;
        c.fg     = 0x1c1c1c;
        c.prompt = 0x0b6e4f; // dark teal/green
        c.input  = 0x000000;
        c.cursor = 0x000000;
    }
    return c;
}

void repl_apply_global_scheme(bool dark) {
    Fl::scheme("gtk+");
    if (dark) {
        Fl::background(0x2d, 0x2d, 0x2d);
        Fl::background2(0x1e, 0x1e, 0x1e);
        Fl::foreground(0xd4, 0xd4, 0xd4);
    } else {
        Fl::background(0xe6, 0xe6, 0xe6);
        Fl::background2(0xff, 0xff, 0xff);
        Fl::foreground(0x10, 0x10, 0x10);
    }
    Fl::reload_scheme();
}
