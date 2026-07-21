#include "Waveform.h"
#include "AudioMetrics.h"

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
    ReplAudioMetrics metrics;
    if (!repl_measure_audio(samples.data(), frames, 1, 0, metrics)) return 1;
    if (std::fabs(metrics.rms - 0.565685f) > 0.001f) return 2;
    if (std::fabs(metrics.peakDbfs - -1.9382f) > 0.02f) return 3;
    if (std::fabs(metrics.crestDb - 3.0103f) > 0.02f) return 4;
    if (std::fabs(metrics.dc) > 0.001f) return 5;
    if (!repl_render_waveform_rgb(samples.data(), frames, 1, 0,
                                  width, height, "wave 12", 200, 800, rgb,
                                  48000.0f)) {
        return 6;
    }
    if (rgb.size() != static_cast<size_t>(width) * height * 3) return 7;

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
    if (waveformPixels < 500) return 8;
    if (loopPixels < 100) return 9;
    if (labelPixels < 40) return 10;

    std::vector<ReplVectorLabel> labels;
    if (!repl_render_waveform_rgb(samples.data(), frames, 1, 0,
                                  width, height, "wave 12", 200, 800, rgb,
                                  48000.0f, &labels)) {
        return 11;
    }
    if (labels.size() < 4 || labels.front().text != "wave 12")
        return 12;

    if (repl_render_waveform_rgb(samples.data(), frames, 1, 1,
                                 width, height, "bad", -1, -1, rgb)) {
        return 13;
    }
    return 0;
}
