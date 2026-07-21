#include "SpectrogramBridge.h"

#include "repl/bitmap_win.h"
#include <skred/api.h>

#include <FL/Fl.H>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
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
    float sampleRate;
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
        bitmap_win_set_waveform_ex(window, request->samples.data(),
                                   static_cast<int>(request->samples.size()),
                                   1, 0, kImageWidth, kImageHeight,
                                   request->title.c_str(), request->loopStart,
                                   request->loopEnd, request->sampleRate) :
        bitmap_win_set_spectrogram_labeled_ex(
            window, request->samples.data(),
            static_cast<int>(request->samples.size()),
            1, 0, kImageWidth, kImageHeight, request->title.c_str(),
            request->sampleRate);
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
        request->sampleRate = 0.0f;
        if (call->argc > 1 && std::isfinite(call->arg[1]))
            request->sampleRate = static_cast<float>(call->arg[1]);
        if (call->argc > 2 && std::isfinite(call->arg[2]))
            request->loopStart = static_cast<int>(call->arg[2]);
        if (call->argc > 3 && std::isfinite(call->arg[3]))
            request->loopEnd = static_cast<int>(call->arg[3]);
        request->samples.resize(static_cast<size_t>(call->data_len));
        for (int i = 0; i < call->data_len; ++i) {
            const double sample = call->data[i];
            request->samples[static_cast<size_t>(i)] =
                std::isfinite(sample) ? static_cast<float>(sample) : 0.0f;
        }
        request->title = call->string && call->string[0] ?
            call->string : (request->kind == RenderKind::Waveform ?
                            "waveform" : "spectrogram");
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

float audioSampleRate() {
    const char *status = skred_audio_status();
    if (!status) return 0.0f;
    const char *line = std::strstr(status, "rate: ");
    unsigned int rate = 0;
    return line && std::sscanf(line, "rate: %u", &rate) == 1 ?
        static_cast<float>(rate) : 0.0f;
}

bool waveParameter(int wave, int parameter, double &value) {
    char command[64];
    std::snprintf(command, sizeof(command), "W*%d,%d", wave, parameter);
    skred_command(command);
    const char *log = skred_log();
    int returnedWave = -1;
    int returnedParameter = -1;
    double returnedValue = 0.0;
    if (!log || std::sscanf(log, "# W* %d %d -> %lf", &returnedWave,
                            &returnedParameter, &returnedValue) != 3 ||
        returnedWave != wave || returnedParameter != parameter) {
        return false;
    }
    value = returnedValue;
    return true;
}

void waveMetadata(int wave, float &sampleRate, int &loopStart, int &loopEnd) {
    sampleRate = audioSampleRate();
    loopStart = -1;
    loopEnd = -1;
    double value = 0.0;
    if (waveParameter(wave, 1, value) && value > 0.0)
        sampleRate = static_cast<float>(value);
    if (waveParameter(wave, 3, value)) loopStart = static_cast<int>(value);
    if (waveParameter(wave, 4, value)) loopEnd = static_cast<int>(value);
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
    char command[192];
    float sampleRate;
    int loopStart, loopEnd;
    waveMetadata(wave, sampleRate, loopStart, loopEnd);
    std::snprintf(command, sizeof(command),
                  "[wavetable %d spectrum] () w>d%d /ff%d 0 %.9g %d %d",
                  wave, wave, kForeignSlot, sampleRate, loopStart, loopEnd);
    return submit(command);
}

int skred_spectrogram_record(int channel) {
    if (channel < -1 || channel > 1) return -1;
    char command[192];
    const float sampleRate = audioSampleRate();
    const char *label = channel < 0 ? "downmix" :
                        (channel == 0 ? "channel 0" : "channel 1");
    std::snprintf(command, sizeof(command),
                  "[record %s spectrum] () r>d%d /ff%d 0 %.9g -1 -1",
                  label, channel, kForeignSlot, sampleRate);
    return submit(command);
}

int skred_waveform_wave(int wave) {
    if (wave < 0) return -1;
    char command[192];
    float sampleRate;
    int loopStart, loopEnd;
    waveMetadata(wave, sampleRate, loopStart, loopEnd);
    std::snprintf(command, sizeof(command),
                  "[wavetable %d waveform] () w>d%d /ff%d 1 %.9g %d %d",
                  wave, wave, kForeignSlot, sampleRate, loopStart, loopEnd);
    return submit(command);
}

int skred_waveform_record(int channel) {
    if (channel < -1 || channel > 1) return -1;
    char command[192];
    const float sampleRate = audioSampleRate();
    const char *label = channel < 0 ? "downmix" :
                        (channel == 0 ? "channel 0" : "channel 1");
    std::snprintf(command, sizeof(command),
                  "[record %s waveform] () r>d%d /ff%d 1 %.9g -1 -1",
                  label, channel, kForeignSlot, sampleRate);
    return submit(command);
}

} // extern "C"
