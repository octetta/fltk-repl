#pragma once

struct ReplAudioMetrics {
    float minimum = 0.0f;
    float maximum = 0.0f;
    float peak = 0.0f;
    float peakToPeak = 0.0f;
    float rms = 0.0f;
    float dc = 0.0f;
    float peakDbfs = -120.0f;
    float crestDb = 0.0f;
    float zeroCrossingPercent = 0.0f;
};

bool repl_measure_audio(const float *samples, int frames, int channels,
                        int channel, ReplAudioMetrics &metrics);
