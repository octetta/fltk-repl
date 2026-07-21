#pragma once

#include <cstdint>
#include <vector>

bool repl_render_spectrogram_rgb(const float *samples, int frames,
                                 int channels, int channel,
                                 int width, int height,
                                 std::vector<uint8_t> &rgb);

void repl_annotate_spectrogram_rgb(std::vector<uint8_t> &rgb,
                                   int width, int height,
                                   const char *title);
