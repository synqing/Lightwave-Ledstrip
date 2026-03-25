#pragma once

#include <cstdint>

#include "ControlBus.h"
#include "MusicalGrid.h"
#include "../../plugins/api/OnsetContext.h"

namespace lightwaveos::audio {

enum class OnsetSemanticChannelIndex : uint8_t {
    Beat = 0,
    Downbeat = 1,
    Transient = 2,
    Kick = 3,
    Snare = 4,
    Hihat = 5,
    Count = 6,
};

struct OnsetSemanticTrackerEntry {
    uint32_t lastFireMs = 0;
    uint32_t previousFireMs = 0;
    uint32_t sequence = 0;
    float heldLevel01 = 0.0f;
};

struct OnsetSemanticTrackerState {
    OnsetSemanticTrackerEntry channels[static_cast<uint8_t>(OnsetSemanticChannelIndex::Count)] = {};
};

struct OnsetSemanticInputs {
    const ControlBusFrame& controlBus;
    const MusicalGridSnapshot& musicalGrid;
    bool audioAvailable = false;
    bool trinityActive = false;
    uint32_t nowMs = 0;
    float dtSeconds = 0.0f;
};

void resetOnsetSemanticTracker(OnsetSemanticTrackerState& state);

void updateOnsetContext(const OnsetSemanticInputs& inputs,
                        OnsetSemanticTrackerState& state,
                        plugins::OnsetContext& out);

} // namespace lightwaveos::audio
