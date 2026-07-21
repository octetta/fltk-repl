#include "Spectrogram.h"

#include <cmath>
#include <cstdint>
#include <vector>

int main() {
    constexpr int frames = 1024;
    constexpr int width = 256;
    constexpr int height = 128;
    std::vector<float> samples(frames);
    for (int i = 0; i < frames; ++i) {
        samples[i] = std::sin(2.0 * 3.14159265358979323846 * 32.0 * i / frames);
    }

    std::vector<uint8_t> rgb;
    if (!repl_render_spectrogram_rgb(samples.data(), frames, 1, 0,
                                     width, height, rgb)) {
        return 1;
    }
    if (rgb.size() != static_cast<size_t>(width) * height * 3) return 2;

    uint8_t brightest = 0;
    for (uint8_t value : rgb) {
        if (value > brightest) brightest = value;
    }
    if (brightest < 200) return 3;

    const std::vector<uint8_t> unlabelled = rgb;
    repl_annotate_spectrogram_rgb(rgb, width, height, "WAVE 42");
    size_t changed = 0;
    for (size_t i = 0; i < rgb.size(); ++i) {
        if (rgb[i] != unlabelled[i]) ++changed;
    }
    if (changed < 100) return 4;

    if (repl_render_spectrogram_rgb(samples.data(), frames, 1, 1,
                                    width, height, rgb)) {
        return 5;
    }
    return 0;
}
