#include "OnsetSemantics.h"

#include <math.h>

namespace lightwaveos::audio {

namespace {

static float clampUnit(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static void populateChannel(plugins::OnsetChannel& channel,
                            OnsetSemanticTrackerState& state,
                            OnsetSemanticChannelIndex index,
                            bool fired,
                            float strength01,
                            float level01,
                            bool reliable,
                            uint32_t nowMs) {
    auto& tracker = state.channels[static_cast<uint8_t>(index)];
    channel.fired = fired;
    channel.strength01 = clampUnit(strength01);
    channel.level01 = clampUnit(level01);
    channel.reliable = reliable;

    if (fired) {
        tracker.previousFireMs = tracker.lastFireMs;
        tracker.lastFireMs = nowMs;
        tracker.sequence++;
    }

    channel.sequence = tracker.sequence;
    channel.intervalMs = (tracker.previousFireMs > 0 && tracker.lastFireMs >= tracker.previousFireMs)
        ? (tracker.lastFireMs - tracker.previousFireMs)
        : 0u;
    channel.ageMs = (tracker.sequence > 0 && nowMs >= tracker.lastFireMs)
        ? (nowMs - tracker.lastFireMs)
        : 0u;
}

} // namespace

void resetOnsetSemanticTracker(OnsetSemanticTrackerState& state) {
    for (auto& tracker : state.channels) {
        tracker = OnsetSemanticTrackerEntry{};
    }
}

void updateOnsetContext(const OnsetSemanticInputs& inputs,
                        OnsetSemanticTrackerState& state,
                        plugins::OnsetContext& out) {
    const bool detectorReliable = inputs.audioAvailable && !inputs.trinityActive;
    const bool timingReliable = inputs.audioAvailable && (inputs.musicalGrid.tempo_confidence >= 0.25f);
    const float dtClamped = fmaxf(0.0f, fminf(inputs.dtSeconds, 0.05f));
    const float heldDecay = powf(0.86f, dtClamped * 60.0f);

    out = plugins::OnsetContext{};
    out.phase01 = inputs.musicalGrid.beat_phase01;
    out.bpm = inputs.musicalGrid.bpm_smoothed;
    out.tempoConfidence = inputs.musicalGrid.tempo_confidence;
    out.timingReliable = timingReliable;

    if (detectorReliable) {
        out.raw.flux = inputs.controlBus.onsetFlux;
        out.raw.env = inputs.controlBus.onsetEnv;
        out.raw.event = inputs.controlBus.onsetEvent;
        out.raw.bassFlux = inputs.controlBus.onsetBassFlux;
        out.raw.midFlux = inputs.controlBus.onsetMidFlux;
        out.raw.highFlux = inputs.controlBus.onsetHighFlux;
    }

    auto heldLevel = [&](OnsetSemanticChannelIndex index, float candidate01) {
        auto& tracker = state.channels[static_cast<uint8_t>(index)];
        tracker.heldLevel01 *= heldDecay;
        const float clamped = clampUnit(candidate01);
        if (clamped > tracker.heldLevel01) {
            tracker.heldLevel01 = clamped;
        }
        return tracker.heldLevel01;
    };

    const float beatLevel = timingReliable ? clampUnit(inputs.musicalGrid.beat_strength) : 0.0f;
    const bool beatFired = timingReliable && inputs.musicalGrid.beat_tick;
    const bool downbeatFired = timingReliable && inputs.musicalGrid.downbeat_tick;
    const float downbeatLevel = heldLevel(OnsetSemanticChannelIndex::Downbeat,
                                          downbeatFired ? beatLevel : 0.0f);

    const bool transientFired = detectorReliable && (inputs.controlBus.onsetEvent > 0.0f);
    const float transientLevel = detectorReliable ? clampUnit(inputs.controlBus.onsetEnv) : 0.0f;

    const bool kickFired = detectorReliable && inputs.controlBus.kickTrigger;
    const float kickLevel = heldLevel(
        OnsetSemanticChannelIndex::Kick,
        detectorReliable ? fmaxf(clampUnit(inputs.controlBus.onsetBassFlux),
                                 kickFired ? clampUnit(inputs.controlBus.onsetBassFlux) : 0.0f)
                         : 0.0f);

    const bool snareFired = inputs.audioAvailable && inputs.controlBus.snareTrigger;
    const float snareLevel = heldLevel(
        OnsetSemanticChannelIndex::Snare,
        inputs.audioAvailable ? fmaxf(clampUnit(inputs.controlBus.snareEnergy),
                                      detectorReliable ? clampUnit(inputs.controlBus.onsetMidFlux) : 0.0f)
                              : 0.0f);

    const bool hihatFired = inputs.audioAvailable && inputs.controlBus.hihatTrigger;
    const float hihatLevel = heldLevel(
        OnsetSemanticChannelIndex::Hihat,
        inputs.audioAvailable ? fmaxf(clampUnit(inputs.controlBus.hihatEnergy),
                                      detectorReliable ? clampUnit(inputs.controlBus.onsetHighFlux) : 0.0f)
                              : 0.0f);

    state.channels[static_cast<uint8_t>(OnsetSemanticChannelIndex::Beat)].heldLevel01 = beatLevel;
    state.channels[static_cast<uint8_t>(OnsetSemanticChannelIndex::Transient)].heldLevel01 = transientLevel;

    populateChannel(out.beat, state, OnsetSemanticChannelIndex::Beat,
                    beatFired, beatFired ? beatLevel : 0.0f, beatLevel, timingReliable, inputs.nowMs);
    populateChannel(out.downbeat, state, OnsetSemanticChannelIndex::Downbeat,
                    downbeatFired, downbeatFired ? beatLevel : 0.0f, downbeatLevel, timingReliable, inputs.nowMs);
    populateChannel(out.transient, state, OnsetSemanticChannelIndex::Transient,
                    transientFired, transientFired ? clampUnit(inputs.controlBus.onsetEvent) : 0.0f,
                    transientLevel, detectorReliable, inputs.nowMs);
    populateChannel(out.kick, state, OnsetSemanticChannelIndex::Kick,
                    kickFired, kickFired ? clampUnit(inputs.controlBus.onsetBassFlux) : 0.0f,
                    kickLevel, detectorReliable, inputs.nowMs);
    populateChannel(out.snare, state, OnsetSemanticChannelIndex::Snare,
                    snareFired, snareFired ? snareLevel : 0.0f,
                    snareLevel, inputs.audioAvailable && !inputs.trinityActive, inputs.nowMs);
    populateChannel(out.hihat, state, OnsetSemanticChannelIndex::Hihat,
                    hihatFired, hihatFired ? hihatLevel : 0.0f,
                    hihatLevel, inputs.audioAvailable && !inputs.trinityActive, inputs.nowMs);
}

} // namespace lightwaveos::audio
