#pragma once

#include <cstdint>
#include <vector>
#include "VectorFont.h"

bool repl_render_waveform_rgb(const float *samples, int frames,
                              int channels, int channel,
                              int width, int height, const char *title,
                              int loopStart, int loopEnd,
                              std::vector<uint8_t> &rgb,
                              float sampleRate = 0.0f,
                              std::vector<ReplVectorLabel> *labels = nullptr);
