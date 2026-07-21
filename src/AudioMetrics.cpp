#include "AudioMetrics.h"

#include <algorithm>
#include <cmath>

namespace {

float sampleAt(const float *samples, int frame, int channels, int channel) {
    if (channel >= 0) {
        const float value = samples[static_cast<size_t>(frame) * channels + channel];
        return std::isfinite(value) ? value : 0.0f;
    }
    double sum = 0.0;
    for (int i = 0; i < channels; ++i) {
        const float value = samples[static_cast<size_t>(frame) * channels + i];
        if (std::isfinite(value)) sum += value;
    }
    return static_cast<float>(sum / channels);
}

float amplitudeDb(float amplitude) {
    return amplitude > 1.0e-6f ? 20.0f * std::log10(amplitude) : -120.0f;
}

} // namespace

bool repl_measure_audio(const float *samples, int frames, int channels,
                        int channel, ReplAudioMetrics &metrics) {
    metrics = {};
    if (!samples || frames <= 0 || channels <= 0 || channel < -1 ||
        channel >= channels) return false;

    double sum = 0.0;
    double sumSquares = 0.0;
    float previous = sampleAt(samples, 0, channels, channel);
    metrics.minimum = previous;
    metrics.maximum = previous;
    int crossings = 0;
    for (int frame = 0; frame < frames; ++frame) {
        const float value = sampleAt(samples, frame, channels, channel);
        metrics.minimum = std::min(metrics.minimum, value);
        metrics.maximum = std::max(metrics.maximum, value);
        sum += value;
        sumSquares += static_cast<double>(value) * value;
        if (frame > 0 && ((previous < 0.0f && value >= 0.0f) ||
                          (previous >= 0.0f && value < 0.0f))) {
            ++crossings;
        }
        previous = value;
    }
    metrics.peak = std::max(std::fabs(metrics.minimum),
                            std::fabs(metrics.maximum));
    metrics.peakToPeak = metrics.maximum - metrics.minimum;
    metrics.dc = static_cast<float>(sum / frames);
    metrics.rms = static_cast<float>(std::sqrt(sumSquares / frames));
    metrics.peakDbfs = amplitudeDb(metrics.peak);
    metrics.crestDb = metrics.rms > 1.0e-9f ?
        20.0f * std::log10(metrics.peak / metrics.rms) : 0.0f;
    metrics.zeroCrossingPercent = frames > 1 ?
        100.0f * crossings / static_cast<float>(frames - 1) : 0.0f;
    return true;
}
