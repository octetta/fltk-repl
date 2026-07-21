#include "Spectrogram.h"
#include "VectorFont.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kNoiseFloorDb = -60.0f;

int nextPowerOfTwo(int value) {
    int result = 1;
    while (result < value && result < 4096) result <<= 1;
    return result;
}

void fft(std::vector<std::complex<float>> &values) {
    const int count = static_cast<int>(values.size());
    for (int i = 1, j = 0; i < count; ++i) {
        int bit = count >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }

    for (int length = 2; length <= count; length <<= 1) {
        const float angle = -2.0f * kPi / static_cast<float>(length);
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (int offset = 0; offset < count; offset += length) {
            std::complex<float> weight(1.0f, 0.0f);
            for (int i = 0; i < length / 2; ++i) {
                const std::complex<float> even = values[offset + i];
                const std::complex<float> odd =
                    values[offset + i + length / 2] * weight;
                values[offset + i] = even + odd;
                values[offset + i + length / 2] = even - odd;
                weight *= step;
            }
        }
    }
}

void heatColor(float level, uint8_t *pixel) {
    static constexpr float stops[][3] = {
        {0.0f, 0.0f, 4.0f},
        {87.0f, 16.0f, 110.0f},
        {188.0f, 55.0f, 84.0f},
        {249.0f, 142.0f, 8.0f},
        {252.0f, 255.0f, 164.0f},
    };
    level = std::clamp(level, 0.0f, 1.0f);
    const float position = level * 4.0f;
    const int first = std::min(static_cast<int>(position), 3);
    const float fraction = position - static_cast<float>(first);
    for (int component = 0; component < 3; ++component) {
        const float value = stops[first][component] +
            (stops[first + 1][component] - stops[first][component]) * fraction;
        pixel[component] = static_cast<uint8_t>(value + 0.5f);
    }
}

float sampleAt(const float *samples, int frame, int channels, int channel) {
    if (channel >= 0) {
        const float value = samples[static_cast<size_t>(frame) * channels + channel];
        return std::isfinite(value) ? value : 0.0f;
    }

    float sum = 0.0f;
    for (int i = 0; i < channels; ++i) {
        const float value = samples[static_cast<size_t>(frame) * channels + i];
        if (std::isfinite(value)) sum += value;
    }
    return sum / static_cast<float>(channels);
}

} // namespace

void repl_annotate_spectrogram_rgb(std::vector<uint8_t> &rgb,
                                   int width, int height,
                                   const char *title) {
    if (width < 240 || height < 120 ||
        rgb.size() != static_cast<size_t>(width) * height * 3) return;

    const int bandHeight = std::clamp(height / 11, 24, 38);
    auto darkenBand = [&](int top, int bottom) {
        for (int y = top; y < bottom; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t *pixel = &rgb[(static_cast<size_t>(y) * width + x) * 3];
                pixel[0] = static_cast<uint8_t>(pixel[0] * 0.28f);
                pixel[1] = static_cast<uint8_t>(pixel[1] * 0.28f);
                pixel[2] = static_cast<uint8_t>(pixel[2] * 0.28f);
            }
        }
    };
    darkenBand(0, bandHeight);
    darkenBand(height - bandHeight, height);

    const int cellHeight = bandHeight - 6;
    repl_draw_vector_text_rgb(rgb, width, height,
                              title && *title ? title : "SPECTROGRAM",
                              8, 3, cellHeight, 226, 246, 255);

    const char *frequency = "FREQ ^";
    repl_draw_vector_text_rgb(rgb, width, height, frequency,
                              width - repl_vector_text_width(frequency, cellHeight) - 8,
                              3, cellHeight, 148, 224, 255);

    const char *time = "TIME >";
    repl_draw_vector_text_rgb(rgb, width, height, time,
                              width - repl_vector_text_width(time, cellHeight) - 8,
                              height - bandHeight + 3, cellHeight,
                              255, 200, 112);
}

bool repl_render_spectrogram_rgb(const float *samples, int frames,
                                 int channels, int channel,
                                 int width, int height,
                                 std::vector<uint8_t> &rgb) {
    rgb.clear();
    if (!samples || frames <= 0 || channels <= 0 || channel < -1 ||
        channel >= channels || width <= 0 || height <= 0 ||
        width > 4096 || height > 2048) {
        return false;
    }
    const size_t pixels = static_cast<size_t>(width) * height;
    if (pixels > std::numeric_limits<size_t>::max() / 3) return false;

    try {
        int fftSize = nextPowerOfTwo(height * 2);
        fftSize = std::clamp(fftSize, 64, 4096);
        const int half = fftSize / 2;
        std::vector<float> window(static_cast<size_t>(fftSize));
        std::vector<std::complex<float>> buffer(static_cast<size_t>(fftSize));
        std::vector<float> magnitudes(pixels);

        for (int i = 0; i < fftSize; ++i) {
            window[i] = 0.5f - 0.5f *
                std::cos(2.0f * kPi * i / static_cast<float>(fftSize - 1));
        }

        float maximum = 1.0e-9f;
        for (int x = 0; x < width; ++x) {
            const int center = width == 1 ? frames / 2 :
                static_cast<int>((static_cast<int64_t>(x) * (frames - 1)) /
                                 (width - 1));
            const int start = center - fftSize / 2;
            for (int i = 0; i < fftSize; ++i) {
                const int frame = start + i;
                const float sample = frame >= 0 && frame < frames ?
                    sampleAt(samples, frame, channels, channel) : 0.0f;
                buffer[i] = std::complex<float>(sample * window[i], 0.0f);
            }
            fft(buffer);
            for (int y = 0; y < height; ++y) {
                int bin = static_cast<int>((static_cast<int64_t>(y) * half) /
                                           height);
                if (bin >= half) bin = half - 1;
                const float magnitude = std::abs(buffer[bin]);
                magnitudes[static_cast<size_t>(y) * width + x] = magnitude;
                maximum = std::max(maximum, magnitude);
            }
        }

        rgb.resize(pixels * 3);
        for (int y = 0; y < height; ++y) {
            const int destinationY = height - 1 - y;
            for (int x = 0; x < width; ++x) {
                const float magnitude =
                    magnitudes[static_cast<size_t>(y) * width + x];
                const float db = 20.0f *
                    std::log10(magnitude / maximum + 1.0e-9f);
                const float level = (db - kNoiseFloorDb) / -kNoiseFloorDb;
                uint8_t *pixel = &rgb[(static_cast<size_t>(destinationY) *
                                      width + x) * 3];
                heatColor(level, pixel);
            }
        }
    } catch (...) {
        rgb.clear();
        return false;
    }
    return true;
}
