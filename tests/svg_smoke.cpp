#include <FL/Fl_SVG_Image.H>

int main() {
    static const char svg[] =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"16\" "
        "viewBox=\"0 0 24 16\"><rect width=\"24\" height=\"16\" "
        "fill=\"#336699\"/></svg>";

    Fl_SVG_Image image("repl-svg-smoke", svg);
    if (image.fail() != 0) return 1;
    if (image.w() != 24 || image.h() != 16) return 2;

    image.normalize();
    return image.array ? 0 : 3;
}
