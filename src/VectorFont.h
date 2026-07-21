#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ReplVectorLabel {
    std::string text;
    int x = 0;
    int y = 0;
    int cellHeight = 0;
    uint8_t red = 255;
    uint8_t green = 255;
    uint8_t blue = 255;
};

int repl_vector_text_width(const char *text, int cellHeight);
void repl_draw_vector_text_rgb(std::vector<uint8_t> &rgb, int width, int height,
                               const char *text, int x, int y, int cellHeight,
                               uint8_t red, uint8_t green, uint8_t blue);
void repl_draw_vector_text_fltk(const char *text, float x, float y,
                                float cellHeight, uint8_t red,
                                uint8_t green, uint8_t blue);
