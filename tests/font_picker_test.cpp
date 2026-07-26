#include "repl/repl_api.h"
#include "repl/font_picker.h"

#include <iostream>
#include <cassert>
#include <cstring>

static std::string g_applied_font;
static int g_applied_size = 0;

static void font_picker_cb(const char *font_name, int size, void *) {
    if (font_name) g_applied_font = font_name;
    g_applied_size = size;
}

int main() {
    repl_ctx *ctx = repl_create("Font Picker Test", 400, 300);
    assert(ctx != nullptr);

    font_picker_win *win = repl_font_picker_open(ctx);
    assert(win != nullptr);

    repl_font_picker_set_handler(win, font_picker_cb, nullptr);

    repl_font_picker_close(win);
    repl_destroy(ctx);

    std::cout << "font_picker_test passed cleanly!" << std::endl;
    return 0;
}
