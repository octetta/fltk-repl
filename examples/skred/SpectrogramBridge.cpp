#include "SpectrogramBridge.h"

#include "repl/bitmap_win.h"
#include <skred/api.h>

#include <FL/Fl.H>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <new>
#include <string>
#include <vector>

namespace {

constexpr int kForeignSlot = 9;
constexpr int kImageWidth = 768;
constexpr int kImageHeight = 384;
constexpr int kMaximumSamples = 16 * 1024 * 1024;

enum class RenderKind {
    Spectrogram = 0,
    Waveform = 1,
};

struct RenderRequest {
    std::vector<float> samples;
    std::string title;
    RenderKind kind;
    int loopStart;
    int loopEnd;
};

std::atomic<unsigned long> gCallbackCount{0};
std::atomic<int> gLastAccepted{0};

void renderAudioBitmap(void *opaque) {
    RenderRequest *request = static_cast<RenderRequest *>(opaque);
    if (!request) return;

    const bool waveform = request->kind == RenderKind::Waveform;
    bitmap_win_t *window = bitmap_win_get(waveform ? "waveform" : "spectrogram");
    bitmap_win_set_title(window, request->title.c_str());
    const int result = waveform ?
        bitmap_win_set_waveform(window, request->samples.data(),
                                static_cast<int>(request->samples.size()),
                                1, 0, kImageWidth, kImageHeight,
                                request->title.c_str(), request->loopStart,
                                request->loopEnd) :
        bitmap_win_set_spectrogram_labeled(
            window, request->samples.data(),
            static_cast<int>(request->samples.size()),
            1, 0, kImageWidth, kImageHeight, request->title.c_str());
    if (result == 0) {
        bitmap_win_show(window);
    }
    delete request;
}

int receiveSamples(const skred_foreign_call_t *call, void *) {
    gCallbackCount.fetch_add(1, std::memory_order_relaxed);
    gLastAccepted.store(0, std::memory_order_relaxed);
    if (!call || !call->data || call->data_len <= 0 ||
        call->data_len > kMaximumSamples) {
        return 0;
    }

    RenderRequest *request = new (std::nothrow) RenderRequest();
    if (!request) return 0;
    try {
        int kind = 0;
        if (call->argc > 0) kind = static_cast<int>(call->arg[0]);
        request->kind = kind == 1 ? RenderKind::Waveform : RenderKind::Spectrogram;
        request->loopStart = -1;
        request->loopEnd = -1;
        request->samples.resize(static_cast<size_t>(call->data_len));
        for (int i = 0; i < call->data_len; ++i) {
            const double sample = call->data[i];
            request->samples[static_cast<size_t>(i)] =
                std::isfinite(sample) ? static_cast<float>(sample) : 0.0f;
        }
        request->title = call->string && call->string[0] ?
            call->string : (request->kind == RenderKind::Waveform ?
                            "Waveform" : "Spectrogram");
    } catch (...) {
        delete request;
        return 0;
    }

    gLastAccepted.store(1, std::memory_order_release);
    Fl::awake(renderAudioBitmap, request);
    return 0;
}

int submit(const char *command) {
    const unsigned long before =
        gCallbackCount.load(std::memory_order_acquire);
    gLastAccepted.store(0, std::memory_order_release);

    std::vector<char> mutableCommand;
    try {
        const char *end = command;
        while (*end) ++end;
        mutableCommand.assign(command, end + 1);
    } catch (...) {
        return -1;
    }
    skred_command(mutableCommand.data());

    return gCallbackCount.load(std::memory_order_acquire) != before &&
           gLastAccepted.load(std::memory_order_acquire) ? 0 : -1;
}

} // namespace

extern "C" {

int skred_spectrogram_bind(void) {
    /* Enable FLTK's cross-thread awake queue before Skred can invoke the
     * reserved callback from a control-dispatch thread. */
    Fl::lock();
    return skred_foreign_function_bind(kForeignSlot, receiveSamples, nullptr);
}

void skred_spectrogram_unbind(void) {
    skred_foreign_function_clear(kForeignSlot);
}

int skred_spectrogram_wave(int wave) {
    if (wave < 0) return -1;
    char command[128];
    std::snprintf(command, sizeof(command),
                  "[Wavetable %d spectrum] () w>d%d /ff%d 0",
                  wave, wave, kForeignSlot);
    return submit(command);
}

int skred_spectrogram_record(int channel) {
    if (channel < -1 || channel > 1) return -1;
    char command[128];
    const char *label = channel < 0 ? "downmix" :
                        (channel == 0 ? "channel 0" : "channel 1");
    std::snprintf(command, sizeof(command),
                  "[Record %s spectrum] () r>d%d /ff%d 0",
                  label, channel, kForeignSlot);
    return submit(command);
}

int skred_waveform_wave(int wave) {
    if (wave < 0) return -1;
    char command[128];
    std::snprintf(command, sizeof(command),
                  "[Wavetable %d waveform] () w>d%d /ff%d 1",
                  wave, wave, kForeignSlot);
    return submit(command);
}

int skred_waveform_record(int channel) {
    if (channel < -1 || channel > 1) return -1;
    char command[128];
    const char *label = channel < 0 ? "downmix" :
                        (channel == 0 ? "channel 0" : "channel 1");
    std::snprintf(command, sizeof(command),
                  "[Record %s waveform] () r>d%d /ff%d 1",
                  label, channel, kForeignSlot);
    return submit(command);
}

} // extern "C"
