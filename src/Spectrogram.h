#pragma once

#include <cstdint>
#include <vector>
#include "VectorFont.h"

struct ReplSpectralMetrics {
    float peakNyquist = 0.0f;
    float centroidNyquist = 0.0f;
    float bandwidthNyquist = 0.0f;
    float rolloff85Nyquist = 0.0f;
    float flatnessDb = -120.0f;
};

bool repl_render_spectrogram_rgb(const float *samples, int frames,
                                 int channels, int channel,
                                 int width, int height,
                                 std::vector<uint8_t> &rgb,
                                 ReplSpectralMetrics *metrics = nullptr);

void repl_annotate_spectrogram_rgb(std::vector<uint8_t> &rgb,
                                   int width, int height,
                                   const char *title, const float *samples,
                                   int frames, int channels, int channel,
                                   const ReplSpectralMetrics *spectralMetrics,
                                   float sampleRate = 0.0f,
                                   std::vector<ReplVectorLabel> *labels = nullptr);
