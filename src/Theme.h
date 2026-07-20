#pragma once
#include <FL/Fl.H>
#include <FL/Enumerations.H>

struct ReplColors {
    unsigned int bg;     // scrollback background, 0xRRGGBB
    unsigned int fg;     // normal output text
    unsigned int prompt; // prompt text ("> ")
    unsigned int input;  // echoed user input text
    unsigned int cursor;
};

// Returns the default palette for a theme.
ReplColors repl_theme_defaults(bool dark);

// Applies FLTK global scheme + window colors so native dialogs and
// widget chrome roughly match the chosen theme too.
void repl_apply_global_scheme(bool dark);

Fl_Color repl_rgb_to_flcolor(unsigned int rgb);
