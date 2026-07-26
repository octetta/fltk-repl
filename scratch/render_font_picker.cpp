#include "FontPickerWindow.h"
#include "Theme.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <png.h>
#include <vector>
#include <iostream>
#include <cstring>

static void save_rgb_png(const char *filename, int width, int height, const std::vector<unsigned char> &rgb) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return;
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    for (int y = 0; y < height; ++y) {
        png_write_row(png_ptr, (png_bytep)&rgb[y * width * 3]);
    }
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

int main() {
    FontPickerWindow *win = new FontPickerWindow(580, 440, "Font Chooser");
    win->show();
    for (int i = 0; i < 10; ++i) {
        Fl::check();
        Fl::wait(0.02);
    }

    int w = win->w();
    int h = win->h();

    unsigned char *raw = fl_read_image(NULL, 0, 0, w, h, 0);
    std::vector<unsigned char> rgb(w * h * 3);
    if (raw) {
        memcpy(rgb.data(), raw, w * h * 3);
        delete[] raw;
    }

    save_rgb_png("/home/stewartj/.gemini/antigravity-cli/brain/e8cb3e10-68ce-494c-b953-1b5973ea4f0c/font_picker_before.png", w, h, rgb);
    std::cout << "Rendered font_picker_before.png (" << w << "x" << h << ")" << std::endl;
    return 0;
}
