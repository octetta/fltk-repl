#pragma once

#include <cstdint>
#include <vector>

int repl_vector_text_width(const char *text, int cellHeight);
void repl_draw_vector_text_rgb(std::vector<uint8_t> &rgb, int width, int height,
                               const char *text, int x, int y, int cellHeight,
                               uint8_t red, uint8_t green, uint8_t blue);
