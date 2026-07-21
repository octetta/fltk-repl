#include "VectorFont.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> render(const char *text) {
    constexpr int width = 384;
    constexpr int height = 32;
    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3, 0);
    repl_draw_vector_text_rgb(rgb, width, height, text, 0, 0, height,
                              255, 255, 255);
    return rgb;
}

} // namespace

int main() {
    const char *supported =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        ".,:;+-/\\()[]<>=%_|'\"^";
    const std::vector<uint8_t> fallback = render("@");
    for (const char *glyph = supported; *glyph; ++glyph) {
        const std::string text(1, *glyph);
        const std::vector<uint8_t> rendered = render(text.c_str());
        if (std::none_of(rendered.begin(), rendered.end(),
                         [](uint8_t value) { return value != 0; })) {
            return 1;
        }
        if (rendered == fallback) return 2;
    }
    if (render(" ") != std::vector<uint8_t>(384 * 32 * 3, 0)) return 3;
    if (render("a") == render("A")) return 4;
    if (render("gjpqy") == render("GJPQY")) return 5;
    return 0;
}
