#include "VectorFont.h"

#include <FL/fl_draw.H>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float kGlyphHeight = 16.0f;
constexpr float kAdvance = 12.0f;
constexpr float kRenderScale = 0.89f;

struct Painter {
    std::vector<uint8_t> &rgb;
    int width;
    int height;
    float originX;
    float originY;
    float scale;
    float thickness;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    void blendPixel(int x, int y, float alpha) {
        if (x < 0 || y < 0 || x >= width || y >= height || alpha <= 0.0f)
            return;
        alpha = std::min(alpha, 1.0f);
        uint8_t *pixel = &rgb[(static_cast<size_t>(y) * width + x) * 3];
        pixel[0] = static_cast<uint8_t>(pixel[0] +
            (red - pixel[0]) * alpha + 0.5f);
        pixel[1] = static_cast<uint8_t>(pixel[1] +
            (green - pixel[1]) * alpha + 0.5f);
        pixel[2] = static_cast<uint8_t>(pixel[2] +
            (blue - pixel[2]) * alpha + 0.5f);
    }

    void disc(float cx, float cy, float radius, float alpha) {
        const int left = static_cast<int>(std::floor(cx - radius));
        const int right = static_cast<int>(std::ceil(cx + radius));
        const int top = static_cast<int>(std::floor(cy - radius));
        const int bottom = static_cast<int>(std::ceil(cy + radius));
        const float radiusSquared = radius * radius;
        for (int py = top; py <= bottom; ++py) {
            for (int px = left; px <= right; ++px) {
                const float dx = px + 0.5f - cx;
                const float dy = py + 0.5f - cy;
                if (dx * dx + dy * dy <= radiusSquared)
                    blendPixel(px, py, alpha);
            }
        }
    }

    void line(float x0, float y0, float x1, float y1) {
        x0 = originX + x0 * scale;
        y0 = originY + y0 * scale;
        x1 = originX + x1 * scale;
        y1 = originY + y1 * scale;
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const int steps = std::max(1, static_cast<int>(
            std::ceil(std::max(std::fabs(dx), std::fabs(dy)) * 1.5f)));
        for (int i = 0; i <= steps; ++i) {
            const float amount = static_cast<float>(i) / steps;
            const float x = x0 + dx * amount;
            const float y = y0 + dy * amount;
            disc(x, y, thickness * 2.2f, 0.10f);
            disc(x, y, thickness * 1.35f, 0.22f);
            disc(x, y, std::max(0.7f, thickness * 0.55f), 0.92f);
        }
    }

    void dot(float x, float y) {
        const float px = originX + x * scale;
        const float py = originY + y * scale;
        disc(px, py, thickness * 2.2f, 0.12f);
        disc(px, py, std::max(0.8f, thickness * 0.7f), 0.95f);
    }
};

template <typename GlyphPainter>
void drawGlyph(GlyphPainter &painter, char glyph) {
#define L(x0,y0,x1,y1) painter.line(x0, y0, x1, y1)
#define D(x,y) painter.dot(x, y)
    switch (static_cast<unsigned char>(glyph)) {
    case 'a': L(2,8,8,8); L(8,8,8,14); L(8,14,2,14); L(2,14,2,11); L(2,11,8,11); break;
    case 'b': L(1,2,1,14); L(1,8,8,8); L(8,8,8,14); L(8,14,1,14); break;
    case 'c': L(9,8,1,8); L(1,8,1,14); L(1,14,9,14); break;
    case 'd': L(9,2,9,14); L(9,8,2,8); L(2,8,2,14); L(2,14,9,14); break;
    case 'e': L(1,11,9,11); L(9,11,9,8); L(9,8,1,8); L(1,8,1,14); L(1,14,9,14); break;
    case 'f': L(8,2,5,2); L(5,2,5,14); L(2,7,8,7); break;
    case 'g': L(9,8,2,8); L(2,8,2,14); L(2,14,9,14); L(9,8,9,16); L(9,16,2,16); break;
    case 'h': L(1,2,1,14); L(1,8,8,8); L(8,8,8,14); break;
    case 'i': D(5,4); L(5,8,5,14); break;
    case 'j': D(7,4); L(7,8,7,16); L(7,16,2,16); break;
    case 'k': L(1,2,1,14); L(8,8,1,12); L(4,11,9,14); break;
    case 'l': L(4,2,4,14); L(4,14,8,14); break;
    case 'm': L(1,14,1,8); L(1,8,4,8); L(4,8,4,14); L(4,8,8,8); L(8,8,8,14); break;
    case 'n': L(1,14,1,8); L(1,8,8,8); L(8,8,8,14); break;
    case 'o': L(2,8,8,8); L(8,8,8,14); L(8,14,2,14); L(2,14,2,8); break;
    case 'p': L(1,8,1,16); L(1,8,8,8); L(8,8,8,14); L(8,14,1,14); break;
    case 'q': L(9,8,9,16); L(9,8,2,8); L(2,8,2,14); L(2,14,9,14); break;
    case 'r': L(1,14,1,8); L(1,8,8,8); break;
    case 's': L(9,8,1,8); L(1,8,1,11); L(1,11,9,11); L(9,11,9,14); L(9,14,1,14); break;
    case 't': L(5,4,5,14); L(2,8,8,8); L(5,14,9,14); break;
    case 'u': L(1,8,1,14); L(1,14,8,14); L(8,8,8,14); break;
    case 'v': L(1,8,5,14); L(5,14,9,8); break;
    case 'w': L(1,8,2,14); L(2,14,5,11); L(5,11,8,14); L(8,14,9,8); break;
    case 'x': L(1,8,9,14); L(9,8,1,14); break;
    case 'y': L(1,8,5,14); L(9,8,5,14); L(5,14,3,16); L(3,16,1,16); break;
    case 'z': L(1,8,9,8); L(9,8,1,14); L(1,14,9,14); break;
    case 'A': L(1,14,1,6); L(1,6,5,2); L(5,2,9,6); L(9,6,9,14); L(1,9,9,9); break;
    case 'B': L(1,2,1,14); L(1,2,7,2); L(7,2,9,4); L(9,4,9,7); L(9,7,7,8); L(1,8,7,8); L(7,8,9,10); L(9,10,9,12); L(9,12,7,14); L(1,14,7,14); break;
    case 'C': L(9,2,1,2); L(1,2,1,14); L(1,14,9,14); break;
    case 'D': L(1,2,1,14); L(1,2,6,2); L(6,2,9,5); L(9,5,9,11); L(9,11,6,14); L(1,14,6,14); break;
    case 'E': L(9,2,1,2); L(1,2,1,14); L(1,8,7,8); L(1,14,9,14); break;
    case 'F': L(1,2,1,14); L(1,2,9,2); L(1,8,7,8); break;
    case 'G': L(9,2,1,2); L(1,2,1,14); L(1,14,9,14); L(9,14,9,8); L(9,8,5,8); break;
    case 'H': L(1,2,1,14); L(9,2,9,14); L(1,8,9,8); break;
    case 'I': L(3,2,7,2); L(5,2,5,14); L(3,14,7,14); break;
    case 'J': L(9,2,9,12); L(9,12,7,14); L(7,14,3,14); L(3,14,1,12); break;
    case 'K': L(1,2,1,14); L(9,2,1,9); L(3,8,9,14); break;
    case 'L': L(1,2,1,14); L(1,14,9,14); break;
    case 'M': L(1,14,1,2); L(1,2,5,7); L(5,7,9,2); L(9,2,9,14); break;
    case 'N': L(1,14,1,2); L(1,2,9,14); L(9,14,9,2); break;
    case 'O': L(1,2,9,2); L(9,2,9,14); L(9,14,1,14); L(1,14,1,2); break;
    case 'P': L(1,2,1,14); L(1,2,8,2); L(8,2,9,3); L(9,3,9,7); L(9,7,8,8); L(1,8,8,8); break;
    case 'Q': L(1,2,9,2); L(9,2,9,14); L(9,14,1,14); L(1,14,1,2); L(6,11,9,14); break;
    case 'R': L(1,2,1,14); L(1,2,8,2); L(8,2,9,3); L(9,3,9,7); L(9,7,8,8); L(1,8,8,8); L(5,8,9,14); break;
    case 'S': L(9,2,1,2); L(1,2,1,8); L(1,8,9,8); L(9,8,9,14); L(9,14,1,14); break;
    case 'T': L(1,2,9,2); L(5,2,5,14); break;
    case 'U': L(1,2,1,14); L(1,14,9,14); L(9,14,9,2); break;
    case 'V': L(1,2,5,14); L(5,14,9,2); break;
    case 'W': L(1,2,2,14); L(2,14,5,9); L(5,9,8,14); L(8,14,9,2); break;
    case 'X': L(1,2,9,14); L(9,2,1,14); break;
    case 'Y': L(1,2,5,8); L(9,2,5,8); L(5,8,5,14); break;
    case 'Z': L(1,2,9,2); L(9,2,1,14); L(1,14,9,14); break;
    case '0': L(1,2,9,2); L(9,2,9,14); L(9,14,1,14); L(1,14,1,2); L(8,3,2,13); break;
    case '1': L(2,14,8,14); L(5,2,5,14); L(2,5,5,2); break;
    case '2': L(1,2,9,2); L(9,2,9,7); L(9,7,1,14); L(1,14,9,14); break;
    case '3': L(1,2,9,2); L(9,2,5,8); L(5,8,9,8); L(9,8,9,14); L(9,14,1,14); break;
    case '4': L(1,10,8,2); L(8,2,8,14); L(1,10,9,10); break;
    case '5': L(9,2,1,2); L(1,2,1,8); L(1,8,9,8); L(9,8,9,14); L(9,14,1,14); break;
    case '6': L(9,2,1,9); L(1,9,1,14); L(1,14,9,14); L(9,14,9,8); L(9,8,1,8); break;
    case '7': L(1,2,9,2); L(9,2,3,14); break;
    case '8': L(1,2,9,2); L(9,2,9,14); L(9,14,1,14); L(1,14,1,2); L(1,8,9,8); break;
    case '9': L(9,8,1,8); L(1,8,1,2); L(1,2,9,2); L(9,2,9,14); L(9,14,1,14); break;
    case '.': D(5,13); break;
    case ',': L(6,12,4,15); break;
    case ':': D(5,4); D(5,10); break;
    case ';': D(5,4); L(6,10,4,15); break;
    case '+': L(5,3,5,11); L(1,7,9,7); break;
    case '-': L(1,7,9,7); break;
    case '/': L(9,0,1,14); break;
    case '\\': L(1,0,9,14); break;
    case '(': L(7,0,3,4); L(3,4,3,10); L(3,10,7,14); break;
    case ')': L(3,0,7,4); L(7,4,7,10); L(7,10,3,14); break;
    case '[': L(8,0,3,0); L(3,0,3,14); L(3,14,8,14); break;
    case ']': L(2,0,7,0); L(7,0,7,14); L(7,14,2,14); break;
    case '<': L(8,2,2,7); L(2,7,8,12); break;
    case '>': L(2,2,8,7); L(8,7,2,12); break;
    case '=': L(1,5,9,5); L(1,9,9,9); break;
    case '%': L(2,4,4,2); L(4,2,5,3); L(5,3,3,5); L(3,5,2,4); L(9,0,1,14); L(6,11,8,9); L(8,9,9,10); L(9,10,7,12); L(7,12,6,11); break;
    case '_': L(1,14,9,14); break;
    case '|': L(5,0,5,14); break;
    case '\'': L(5,0,4,4); break;
    case '"': L(3,0,3,4); L(7,0,7,4); break;
    case '^': L(1,8,5,3); L(5,3,9,8); break;
    case ' ': break;
    default: L(2,2,8,2); L(8,2,8,14); L(8,14,2,14); L(2,14,2,2); break;
    }
#undef D
#undef L
}

struct FltkPainter {
    float originX;
    float originY;
    float scale;
    float thickness;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    void stroke(float x0, float y0, float x1, float y1, int width,
                float brightness) {
        fl_color(fl_rgb_color(
            static_cast<uchar>(red * brightness),
            static_cast<uchar>(green * brightness),
            static_cast<uchar>(blue * brightness)));
        fl_line_style(FL_SOLID, std::max(1, width));
        fl_line(static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)),
                static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)));
    }

    void line(float x0, float y0, float x1, float y1) {
        x0 = originX + x0 * scale;
        y0 = originY + y0 * scale;
        x1 = originX + x1 * scale;
        y1 = originY + y1 * scale;
        stroke(x0, y0, x1, y1, static_cast<int>(thickness * 3.2f), 0.18f);
        stroke(x0, y0, x1, y1, static_cast<int>(thickness * 1.8f), 0.42f);
        stroke(x0, y0, x1, y1, static_cast<int>(thickness), 1.0f);
    }

    void dot(float x, float y) {
        const float px = originX + x * scale;
        const float py = originY + y * scale;
        const int radius = std::max(1, static_cast<int>(std::lround(thickness)));
        fl_color(fl_rgb_color(red, green, blue));
        fl_pie(static_cast<int>(px) - radius, static_cast<int>(py) - radius,
               radius * 2 + 1, radius * 2 + 1, 0, 360);
    }
};

} // namespace

int repl_vector_text_width(const char *text, int cellHeight) {
    if (!text || !*text || cellHeight <= 0) return 0;
    const float scale = cellHeight * kRenderScale / kGlyphHeight;
    return static_cast<int>(std::strlen(text) * kAdvance * scale + 0.5f);
}

void repl_draw_vector_text_rgb(std::vector<uint8_t> &rgb, int width, int height,
                               const char *text, int x, int y, int cellHeight,
                               uint8_t red, uint8_t green, uint8_t blue) {
    if (!text || !*text || width <= 0 || height <= 0 || cellHeight <= 0 ||
        rgb.size() != static_cast<size_t>(width) * height * 3) return;
    const float scale = cellHeight * kRenderScale / kGlyphHeight;
    Painter painter{rgb, width, height, static_cast<float>(x),
                    y + (cellHeight - kGlyphHeight * scale) * 0.5f,
                    scale, std::max(1.0f, scale * 0.64f), red, green, blue};
    for (const char *cursor = text; *cursor; ++cursor) {
        drawGlyph(painter, *cursor);
        painter.originX += kAdvance * scale;
        if (painter.originX >= width) break;
    }
}

void repl_draw_vector_text_fltk(const char *text, float x, float y,
                                float cellHeight, uint8_t red,
                                uint8_t green, uint8_t blue) {
    if (!text || !*text || cellHeight <= 0.0f) return;
    const float scale = cellHeight * kRenderScale / kGlyphHeight;
    FltkPainter painter{x, y + (cellHeight - kGlyphHeight * scale) * 0.5f,
                        scale, std::max(1.0f, scale * 0.64f),
                        red, green, blue};
    for (const char *cursor = text; *cursor; ++cursor) {
        drawGlyph(painter, *cursor);
        painter.originX += kAdvance * scale;
    }
    fl_line_style(0);
}
