#include "Waveform.h"
#include "VectorFont.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {

struct Canvas {
    std::vector<uint8_t> &rgb;
    int width;
    int height;

    void pixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue,
               float alpha = 1.0f) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        uint8_t *where = &rgb[(static_cast<size_t>(y) * width + x) * 3];
        where[0] = static_cast<uint8_t>(where[0] +
            (red - where[0]) * alpha + 0.5f);
        where[1] = static_cast<uint8_t>(where[1] +
            (green - where[1]) * alpha + 0.5f);
        where[2] = static_cast<uint8_t>(where[2] +
            (blue - where[2]) * alpha + 0.5f);
    }

    void line(int x0, int y0, int x1, int y1,
              uint8_t red, uint8_t green, uint8_t blue,
              float alpha = 1.0f) {
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            pixel(x0, y0, red, green, blue, alpha);
            if (x0 == x1 && y0 == y1) break;
            const int twice = error * 2;
            if (twice >= dy) { error += dy; x0 += sx; }
            if (twice <= dx) { error += dx; y0 += sy; }
        }
    }
};

float sampleAt(const float *samples, int frame, int channels, int channel) {
    if (channel >= 0) {
        const float value = samples[static_cast<size_t>(frame) * channels + channel];
        return std::isfinite(value) ? value : 0.0f;
    }
    float total = 0.0f;
    for (int i = 0; i < channels; ++i) {
        const float value = samples[static_cast<size_t>(frame) * channels + i];
        if (std::isfinite(value)) total += value;
    }
    return total / channels;
}

} // namespace

bool repl_render_waveform_rgb(const float *samples, int frames,
                              int channels, int channel,
                              int width, int height, const char *title,
                              int loopStart, int loopEnd,
                              std::vector<uint8_t> &rgb) {
    rgb.clear();
    if (!samples || frames <= 0 || channels <= 0 || channel < -1 ||
        channel >= channels || width < 240 || height < 120 ||
        width > 4096 || height > 2048) return false;
    const size_t pixels = static_cast<size_t>(width) * height;
    if (pixels > std::numeric_limits<size_t>::max() / 3) return false;

    try {
        rgb.assign(pixels * 3, 0);
        Canvas canvas{rgb, width, height};
        const int top = std::clamp(height / 10, 32, 44);
        const int bottom = std::clamp(height / 11, 28, 38);
        const int left = 48;
        const int right = width - 14;
        const int plotBottom = height - bottom - 1;
        const int plotWidth = right - left + 1;
        const int plotHeight = plotBottom - top + 1;
        const int center = top + plotHeight / 2;

        float minimum = 0.0f;
        float maximum = 0.0f;
        for (int i = 0; i < frames; ++i) {
            const float value = sampleAt(samples, i, channels, channel);
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
        const float range = std::max(1.0f, std::max(std::fabs(minimum),
                                                    std::fabs(maximum)));
        auto sampleY = [&](float value) {
            const float normalized = std::clamp(value / range, -1.0f, 1.0f);
            return center - static_cast<int>(normalized * (plotHeight - 3) * 0.5f);
        };

        for (int division = 0; division <= 4; ++division) {
            const int x = left + division * (plotWidth - 1) / 4;
            canvas.line(x, top, x, plotBottom, 32, 63, 72, 0.65f);
        }
        for (int division = 0; division <= 4; ++division) {
            const int y = top + division * (plotHeight - 1) / 4;
            canvas.line(left, y, right, y, 32, 63, 72, 0.65f);
        }
        canvas.line(left, center, right, center, 86, 132, 142, 0.9f);
        canvas.line(left, top, left, plotBottom, 86, 132, 142, 0.9f);

        int previousY = sampleY(sampleAt(samples, 0, channels, channel));
        for (int column = 0; column < plotWidth; ++column) {
            int first = static_cast<int>((static_cast<int64_t>(column) * frames) /
                                         plotWidth);
            int last = static_cast<int>((static_cast<int64_t>(column + 1) * frames) /
                                        plotWidth);
            if (last <= first) last = first + 1;
            if (last > frames) last = frames;
            float low = sampleAt(samples, first, channels, channel);
            float high = low;
            for (int frame = first + 1; frame < last; ++frame) {
                const float value = sampleAt(samples, frame, channels, channel);
                low = std::min(low, value);
                high = std::max(high, value);
            }
            const int x = left + column;
            const int lowY = sampleY(low);
            const int highY = sampleY(high);
            canvas.line(x, highY, x, lowY, 104, 235, 244, 0.95f);
            const int representativeY = sampleY((low + high) * 0.5f);
            if (column > 0)
                canvas.line(x - 1, previousY, x, representativeY,
                            186, 250, 255, 0.75f);
            previousY = representativeY;
        }

        auto drawLoop = [&](int frame, uint8_t red, uint8_t green,
                            uint8_t blue) {
            if (frame < 0 || frame > frames) return;
            const int x = left + static_cast<int>(
                (static_cast<int64_t>(frame) * (plotWidth - 1)) /
                std::max(frames, 1));
            canvas.line(x, top, x, plotBottom, red, green, blue, 0.9f);
            for (int offset = 0; offset < 5; ++offset)
                canvas.line(x - offset, top + offset, x + offset, top + offset,
                            red, green, blue, 0.95f);
        };
        drawLoop(loopStart, 104, 255, 144);
        drawLoop(loopEnd, 255, 178, 72);

        const int cellHeight = top - 8;
        repl_draw_vector_text_rgb(rgb, width, height,
                                  title && *title ? title : "WAVEFORM",
                                  8, 3, cellHeight, 226, 246, 255);
        char samplesLabel[48];
        std::snprintf(samplesLabel, sizeof(samplesLabel), "N %d", frames);
        repl_draw_vector_text_rgb(rgb, width, height, samplesLabel,
                                  8, height - bottom + 3, bottom - 7,
                                  148, 224, 255);
        char rangeLabel[64];
        std::snprintf(rangeLabel, sizeof(rangeLabel),
                      "MIN %.3f  MAX %.3f", minimum, maximum);
        const int rangeWidth = repl_vector_text_width(rangeLabel, bottom - 7);
        repl_draw_vector_text_rgb(rgb, width, height, rangeLabel,
                                  std::max(8, width - rangeWidth - 8),
                                  height - bottom + 3, bottom - 7,
                                  255, 200, 112);
        if (loopStart >= 0 && loopEnd >= loopStart) {
            char loopLabel[64];
            std::snprintf(loopLabel, sizeof(loopLabel), "LOOP %d-%d",
                          loopStart, loopEnd);
            const int loopWidth = repl_vector_text_width(loopLabel, cellHeight);
            repl_draw_vector_text_rgb(rgb, width, height, loopLabel,
                                      width - loopWidth - 8, 3, cellHeight,
                                      104, 255, 144);
        }
    } catch (...) {
        rgb.clear();
        return false;
    }
    return true;
}
