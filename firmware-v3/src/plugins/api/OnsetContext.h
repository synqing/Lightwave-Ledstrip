#pragma once

#include <cstdint>

namespace lightwaveos::plugins {

struct OnsetRawSignals {
    float flux = 0.0f;
    float env = 0.0f;
    float event = 0.0f;
    float bassFlux = 0.0f;
    float midFlux = 0.0f;
    float highFlux = 0.0f;
};

struct OnsetChannel {
    bool fired = false;
    float strength01 = 0.0f;
    float level01 = 0.0f;
    uint32_t ageMs = 0;
    uint32_t intervalMs = 0;
    uint32_t sequence = 0;
    bool reliable = false;
};

struct OnsetContext {
    OnsetChannel beat{};
    OnsetChannel downbeat{};
    OnsetChannel transient{};
    OnsetChannel kick{};
    OnsetChannel snare{};
    OnsetChannel hihat{};

    OnsetRawSignals raw{};

    float phase01 = 0.0f;
    float bpm = 120.0f;
    float tempoConfidence = 0.0f;
    bool timingReliable = false;
};

} // namespace lightwaveos::plugins
