#include "Spectrogram.h"
#include "AudioMetrics.h"
#include "VectorFont.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <cstdio>

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

int fittedCellHeight(const char *text, int maximumWidth, int maximumHeight) {
    int height = maximumHeight;
    while (height > 7 && repl_vector_text_width(text, height) > maximumWidth)
        --height;
    return height;
}

void drawLabel(std::vector<uint8_t> &rgb, int width, int height,
               std::vector<ReplVectorLabel> *labels, const char *text,
               int x, int y, int cellHeight, uint8_t red, uint8_t green,
               uint8_t blue) {
    if (labels) {
        labels->push_back({text ? text : "", x, y, cellHeight,
                           red, green, blue});
    } else {
        repl_draw_vector_text_rgb(rgb, width, height, text, x, y, cellHeight,
                                  red, green, blue);
    }
}

} // namespace

void repl_annotate_spectrogram_rgb(std::vector<uint8_t> &rgb,
                                   int width, int height,
                                   const char *title, const float *samples,
                                   int frames, int channels, int channel,
                                   const ReplSpectralMetrics *spectralMetrics,
                                   float sampleRate,
                                   std::vector<ReplVectorLabel> *labels) {
    if (width < 240 || height < 120 ||
        rgb.size() != static_cast<size_t>(width) * height * 3) return;

    const int bandHeight = std::clamp(height / 7, 36, 56);
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

    const int cellHeight = bandHeight - 8;
    const char *displayTitle = title && *title ? title : "spectrogram";
    const char *axes = "freq ^  time >";
    const int axesHeight = fittedCellHeight(axes, width / 3, cellHeight);
    const int axesWidth = repl_vector_text_width(axes, axesHeight);
    const int titleHeight = fittedCellHeight(
        displayTitle, std::max(40, width - axesWidth - 24), cellHeight);
    if (labels) labels->clear();
    drawLabel(rgb, width, height, labels, displayTitle,
              8, 3, titleHeight, 226, 246, 255);

    drawLabel(rgb, width, height, labels, axes, width - axesWidth - 8,
              3, axesHeight, 148, 224, 255);

    ReplAudioMetrics audio;
    if (!repl_measure_audio(samples, frames, channels, channel, audio)) return;
    char levelLine[160];
    char spectralLine[200];
    const float rmsDbfs = audio.rms > 1.0e-6f ?
        20.0f * std::log10(audio.rms) : -120.0f;
    if (width >= 560) {
        if (sampleRate > 0.0f) {
            std::snprintf(levelLine, sizeof(levelLine),
                          "n %d  %.2fms  rms %.2fdbfs  peak %.2fdbfs  dc %+.4f",
                          frames, frames * 1000.0f / sampleRate, rmsDbfs,
                          audio.peakDbfs, audio.dc);
        } else {
            std::snprintf(levelLine, sizeof(levelLine),
                          "n %d  rms %.2fdbfs  peak %.2fdbfs  dc %+.4f",
                          frames, rmsDbfs, audio.peakDbfs, audio.dc);
        }
        if (spectralMetrics) {
            if (sampleRate > 0.0f) {
                const float nyquist = sampleRate * 0.5f;
                std::snprintf(spectralLine, sizeof(spectralLine),
                              "fpeak %.1fhz  cent %.1f  bw %.1f  r85 %.1f  flat %.1fdb",
                              spectralMetrics->peakNyquist * nyquist,
                              spectralMetrics->centroidNyquist * nyquist,
                              spectralMetrics->bandwidthNyquist * nyquist,
                              spectralMetrics->rolloff85Nyquist * nyquist,
                              spectralMetrics->flatnessDb);
            } else {
                std::snprintf(spectralLine, sizeof(spectralLine),
                              "fpeak %.3fnyq  cent %.3f  bw %.3f  r85 %.3f  flat %.1fdb",
                              spectralMetrics->peakNyquist,
                              spectralMetrics->centroidNyquist,
                              spectralMetrics->bandwidthNyquist,
                              spectralMetrics->rolloff85Nyquist,
                              spectralMetrics->flatnessDb);
            }
        } else spectralLine[0] = '\0';
    } else {
        if (sampleRate > 0.0f) {
            std::snprintf(levelLine, sizeof(levelLine),
                          "n %d %.1fms rms %.1fdb peak %.1fdb",
                          frames, frames * 1000.0f / sampleRate,
                          rmsDbfs, audio.peakDbfs);
        } else {
            std::snprintf(levelLine, sizeof(levelLine),
                          "n %d rms %.1fdb peak %.1fdb",
                          frames, rmsDbfs, audio.peakDbfs);
        }
        if (spectralMetrics) {
            if (sampleRate > 0.0f) {
                const float nyquist = sampleRate * 0.5f;
                std::snprintf(spectralLine, sizeof(spectralLine),
                              "fpk %.0fhz cent %.0f r85 %.0f",
                              spectralMetrics->peakNyquist * nyquist,
                              spectralMetrics->centroidNyquist * nyquist,
                              spectralMetrics->rolloff85Nyquist * nyquist);
            } else {
                std::snprintf(spectralLine, sizeof(spectralLine),
                              "fpk %.3fn cent %.3fn r85 %.3fn",
                              spectralMetrics->peakNyquist,
                              spectralMetrics->centroidNyquist,
                              spectralMetrics->rolloff85Nyquist);
            }
        } else spectralLine[0] = '\0';
    }
    const int rowHeight = (bandHeight - 5) / 2;
    const int levelHeight = fittedCellHeight(levelLine, width - 16, rowHeight);
    drawLabel(rgb, width, height, labels, levelLine, 8,
              height - bandHeight + 1, levelHeight, 148, 224, 255);
    if (spectralLine[0]) {
        const int spectralHeight = fittedCellHeight(
            spectralLine, width - 16, rowHeight);
        drawLabel(rgb, width, height, labels, spectralLine, 8,
                  height - bandHeight + rowHeight, spectralHeight,
                  255, 200, 112);
    }
}

bool repl_render_spectrogram_rgb(const float *samples, int frames,
                                 int channels, int channel,
                                 int width, int height,
                                 std::vector<uint8_t> &rgb,
                                 ReplSpectralMetrics *metrics) {
    rgb.clear();
    if (metrics) *metrics = {};
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
        std::vector<double> spectrum(metrics ? static_cast<size_t>(half) : 0,
                                     0.0);

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
            if (metrics) {
                for (int bin = 0; bin < half; ++bin)
                    spectrum[static_cast<size_t>(bin)] += std::abs(buffer[bin]);
            }
            for (int y = 0; y < height; ++y) {
                int bin = static_cast<int>((static_cast<int64_t>(y) * half) /
                                           height);
                if (bin >= half) bin = half - 1;
                const float magnitude = std::abs(buffer[bin]);
                magnitudes[static_cast<size_t>(y) * width + x] = magnitude;
                maximum = std::max(maximum, magnitude);
            }
        }

        if (metrics && half > 2) {
            double total = 0.0;
            double weighted = 0.0;
            double logarithmic = 0.0;
            double peakMagnitude = -1.0;
            int peakBin = 1;
            for (int bin = 1; bin < half; ++bin) {
                const double magnitude = spectrum[static_cast<size_t>(bin)] /
                                         std::max(width, 1);
                total += magnitude;
                weighted += magnitude * bin;
                logarithmic += std::log(std::max(magnitude, 1.0e-20));
                if (magnitude > peakMagnitude) {
                    peakMagnitude = magnitude;
                    peakBin = bin;
                }
            }
            if (total > 1.0e-20) {
                const double centroid = weighted / total;
                double variance = 0.0;
                double cumulative = 0.0;
                int rolloffBin = half - 1;
                for (int bin = 1; bin < half; ++bin) {
                    const double magnitude = spectrum[static_cast<size_t>(bin)] /
                                             std::max(width, 1);
                    const double distance = bin - centroid;
                    variance += magnitude * distance * distance;
                    cumulative += magnitude;
                    if (cumulative >= total * 0.85 && rolloffBin == half - 1)
                        rolloffBin = bin;
                }
                const double divisor = static_cast<double>(half);
                metrics->peakNyquist = static_cast<float>(peakBin / divisor);
                metrics->centroidNyquist = static_cast<float>(centroid / divisor);
                metrics->bandwidthNyquist = static_cast<float>(
                    std::sqrt(variance / total) / divisor);
                metrics->rolloff85Nyquist = static_cast<float>(
                    rolloffBin / divisor);
                const double arithmetic = total / (half - 1);
                const double geometric = std::exp(logarithmic / (half - 1));
                metrics->flatnessDb = static_cast<float>(10.0 * std::log10(
                    std::max(geometric / arithmetic, 1.0e-12)));
            } else {
                metrics->flatnessDb = -120.0f;
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
