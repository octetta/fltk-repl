#include "Waveform.h"

#include <cmath>
#include <cstdint>
#include <vector>

int main() {
    constexpr int frames = 1024;
    constexpr int width = 512;
    constexpr int height = 256;
    std::vector<float> samples(frames);
    for (int i = 0; i < frames; ++i) {
        samples[i] = 0.8f * std::sin(
            2.0 * 3.14159265358979323846 * 8.0 * i / frames);
    }

    std::vector<uint8_t> rgb;
    if (!repl_render_waveform_rgb(samples.data(), frames, 1, 0,
                                  width, height, "WAVE 12", 200, 800, rgb)) {
        return 1;
    }
    if (rgb.size() != static_cast<size_t>(width) * height * 3) return 2;

    int waveformPixels = 0;
    int loopPixels = 0;
    int labelPixels = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint8_t *pixel = &rgb[(static_cast<size_t>(y) * width + x) * 3];
            if (pixel[1] > 180 && pixel[2] > 180) ++waveformPixels;
            if (pixel[0] < 150 && pixel[1] > 220 && pixel[2] < 180)
                ++loopPixels;
            if (y < 32 && pixel[0] > 180 && pixel[1] > 180)
                ++labelPixels;
        }
    }
    if (waveformPixels < 500) return 3;
    if (loopPixels < 100) return 4;
    if (labelPixels < 40) return 5;

    if (repl_render_waveform_rgb(samples.data(), frames, 1, 1,
                                 width, height, "BAD", -1, -1, rgb)) {
        return 6;
    }
    return 0;
}
