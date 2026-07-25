#pragma once
#include <FL/Fl.H>
#include <FL/Enumerations.H>
#include <string>

struct ReplColors {
    unsigned int bg;     // scrollback background, 0xRRGGBB
    unsigned int fg;     // normal output text
    unsigned int prompt; // prompt text ("> ")
    unsigned int input;  // echoed user input text
    unsigned int cursor;
};

struct ReplCustomTheme {
    unsigned int bg     = 0xf1f5f9; // #f1f5f9 Canvas
    unsigned int card   = 0xffffff; // #ffffff Card/Inputs
    unsigned int fg     = 0x212529; // #212529 Primary Text
    unsigned int accent = 0x0d6efd; // #0d6efd Primary Accent (Active buttons)
    unsigned int focus  = 0x0d6efd; // #0d6efd Focus ring color
    unsigned int border = 0xcbd5e1; // #cbd5e1 Container borders
    bool is_dark        = false;
};

// Returns default palette for light/dark theme.
ReplColors repl_theme_defaults(bool dark);

// Applies FLTK global scheme + window colors for light/dark
void repl_apply_global_scheme(bool dark);

// Applies custom theme parameters directly
void repl_apply_custom_theme(const ReplCustomTheme &t);

// Retrieves current active custom theme parameters
ReplCustomTheme repl_get_active_custom_theme();

// Load / Save theme configuration file (e.g. ~/.config/skrepl/theme.conf)
bool repl_load_theme_file(const char *filename, ReplCustomTheme &out);
bool repl_save_theme_file(const char *filename, const ReplCustomTheme &t);

// Parse color from hex string ("#0d6efd", "0x0d6efd", "0d6efd")
unsigned int repl_parse_color(const char *str, unsigned int default_val);

Fl_Color repl_rgb_to_flcolor(unsigned int rgb);
