/**
 * @file AudioReactiveLowRiskPackHelpers.h
 * @brief Shared helpers for low-risk ambient -> audio-reactive conversions.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "AudioReactivePolicy.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace lightwaveos::effects::ieffect::lowrisk_ar {

static inline float clamp01f(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

static inline float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline float expAlpha(float dt, float tauS) {
    if (tauS <= 0.0f) return 1.0f;
    return 1.0f - expf(-dt / tauS);
}

static inline float smoothTo(float current, float target, float dt, float tauS) {
    return current + (target - current) * expAlpha(dt, tauS);
}

static inline float decay(float value, float dt, float tauS) {
    if (tauS <= 0.0f) return 0.0f;
    return value * expf(-dt / tauS);
}

static inline float smoothHue(float current, float target, float dt, float tauS) {
    float delta = target - current;
    while (delta > 128.0f) delta -= 256.0f;
    while (delta < -128.0f) delta += 256.0f;
    const float next = current + delta * expAlpha(dt, tauS);
    float wrapped = fmodf(next, 256.0f);
    if (wrapped < 0.0f) wrapped += 256.0f;
    return wrapped;
}

static inline float fallbackSine(uint32_t rawMs, float rate, float phaseOffset = 0.0f) {
    return 0.5f + 0.5f * sinf(static_cast<float>(rawMs) * rate + phaseOffset);
}

static inline float fallbackTriangle(uint32_t rawMs, float rate) {
    const float x = static_cast<float>(rawMs) * rate;
    const float frac = x - floorf(x);
    return 1.0f - fabsf(2.0f * frac - 1.0f);
}

static constexpr uint8_t kNoteHues[12] = {
    0, 12, 24, 40, 56, 74, 92, 112, 134, 154, 178, 202
};

static inline uint8_t dominantNoteFromChroma(const plugins::EffectContext& ctx) {
    if (!ctx.audio.available) return 0;
    const float* scores = ctx.audio.heavyChroma();
    float best = scores[0];
    uint8_t bestIdx = 0;
    for (uint8_t i = 1; i < 12; ++i) {
        if (scores[i] > best) {
            best = scores[i];
            bestIdx = i;
        }
    }
    return bestIdx;
}

static inline uint8_t selectMusicalHue(const plugins::EffectContext& ctx, bool& chordGateOpen) {
    if (!ctx.audio.available) return 24;
    const float conf = ctx.audio.chordConfidence();
    if (conf >= 0.40f) chordGateOpen = true;
    else if (conf <= 0.25f) chordGateOpen = false;

    const uint8_t note = chordGateOpen
        ? static_cast<uint8_t>(ctx.audio.rootNote() % 12)
        : dominantNoteFromChroma(ctx);
    return kNoteHues[note];
}

struct ArRuntimeState {
    float audioPresence = 0.0f;
    float envBass = 0.0f;
    float envMid = 0.0f;
    float envTreble = 0.0f;
    float envFlux = 0.0f;
    float impactBeat = 0.0f;
    float memory = 0.0f;
    float tonalHue = 24.0f;
    float modeSmooth = 2.0f;
    bool chordGateOpen = false;
    uint32_t lastBeatMs = 0;
};

struct ArSignalSnapshot {
    float bass = 0.0f;
    float mid = 0.0f;
    float treble = 0.0f;
    float flux = 0.0f;
    float rhythmic = 0.0f;
    float harmonic = 0.0f;
    float rms = 0.0f;
    float beat = 0.0f;
    bool beatTick = false;
};

struct Ar16Controls {
    enum Index : uint8_t {
        AUDIO_MIX = 0,
        BEAT_GAIN,
        BASS_GAIN,
        MID_GAIN,
        TREBLE_GAIN,
        FLUX_GAIN,
        HARMONIC_GAIN,
        RHYTHMIC_GAIN,
        ATTACK_S,
        RELEASE_S,
        MOTION_RATE,
        MOTION_DEPTH,
        COLOUR_ANCHOR_MIX,
        EVENT_DECAY_S,
        MEMORY_GAIN,
        SILENCE_HOLD,
        COUNT
    };

    float values[COUNT];

    Ar16Controls() {
        resetDefaults();
    }

    void resetDefaults() {
        for (uint8_t i = 0; i < COUNT; ++i) values[i] = kDefaults[i];
    }

    bool set(const char* name, float value) {
        if (!name) return false;
        for (uint8_t i = 0; i < COUNT; ++i) {
            if (strcmp(name, kNames[i]) == 0) {
                values[i] = clampf(value, kMins[i], kMaxs[i]);
                return true;
            }
        }
        return false;
    }

    float get(const char* name) const {
        if (!name) return 0.0f;
        for (uint8_t i = 0; i < COUNT; ++i) {
            if (strcmp(name, kNames[i]) == 0) {
                return values[i];
            }
        }
        return 0.0f;
    }

    static constexpr uint8_t parameterCount() { return COUNT; }

    static const plugins::EffectParameter* parameter(uint8_t index) {
        if (index >= COUNT) return nullptr;
        return &kParameters[index];
    }

    float audioMix() const { return values[AUDIO_MIX]; }
    float beatGain() const { return values[BEAT_GAIN]; }
    float bassGain() const { return values[BASS_GAIN]; }
    float midGain() const { return values[MID_GAIN]; }
    float trebleGain() const { return values[TREBLE_GAIN]; }
    float fluxGain() const { return values[FLUX_GAIN]; }
    float harmonicGain() const { return values[HARMONIC_GAIN]; }
    float rhythmicGain() const { return values[RHYTHMIC_GAIN]; }
    float attackS() const { return values[ATTACK_S]; }
    float releaseS() const { return values[RELEASE_S]; }
    float motionRate() const { return values[MOTION_RATE]; }
    float motionDepth() const { return values[MOTION_DEPTH]; }
    float colourAnchorMix() const { return values[COLOUR_ANCHOR_MIX]; }
    float eventDecayS() const { return values[EVENT_DECAY_S]; }
    float memoryGain() const { return values[MEMORY_GAIN]; }
    float silenceHold() const { return values[SILENCE_HOLD]; }

private:
    static constexpr const char* kNames[COUNT] = {
        "audio_mix",
        "beat_gain",
        "bass_gain",
        "mid_gain",
        "treble_gain",
        "flux_gain",
        "harmonic_gain",
        "rhythmic_gain",
        "attack_s",
        "release_s",
        "motion_rate",
        "motion_depth",
        "colour_anchor_mix",
        "event_decay_s",
        "memory_gain",
        "silence_hold"
    };

    static constexpr float kMins[COUNT] = {
        0.0f,  // audio_mix
        0.0f,  // beat_gain
        0.0f,  // bass_gain
        0.0f,  // mid_gain
        0.0f,  // treble_gain
        0.0f,  // flux_gain
        0.0f,  // harmonic_gain
        0.0f,  // rhythmic_gain
        0.01f, // attack_s
        0.05f, // release_s
        0.25f, // motion_rate
        0.0f,  // motion_depth
        0.0f,  // colour_anchor_mix
        0.05f, // event_decay_s
        0.0f,  // memory_gain
        0.0f   // silence_hold
    };

    static constexpr float kMaxs[COUNT] = {
        1.0f, // audio_mix
        2.0f, // beat_gain
        2.0f, // bass_gain
        2.0f, // mid_gain
        2.0f, // treble_gain
        2.0f, // flux_gain
        2.0f, // harmonic_gain
        2.0f, // rhythmic_gain
        1.50f, // attack_s
        2.50f, // release_s
        3.0f, // motion_rate
        2.0f, // motion_depth
        1.0f, // colour_anchor_mix
        2.0f, // event_decay_s
        2.0f, // memory_gain
        1.0f  // silence_hold
    };

    static constexpr float kDefaults[COUNT] = {
        0.35f, // audio_mix
        0.80f, // beat_gain
        1.00f, // bass_gain
        1.00f, // mid_gain
        1.00f, // treble_gain
        0.90f, // flux_gain
        0.70f, // harmonic_gain
        0.70f, // rhythmic_gain
        0.20f, // attack_s
        0.60f, // release_s
        1.00f, // motion_rate
        0.85f, // motion_depth
        0.65f, // colour_anchor_mix
        0.30f, // event_decay_s
        0.70f, // memory_gain
        0.55f  // silence_hold
    };

    static const plugins::EffectParameter kParameters[COUNT];
};

inline const plugins::EffectParameter Ar16Controls::kParameters[COUNT] = {
    plugins::EffectParameter(kNames[AUDIO_MIX], "Audio Mix", kMins[AUDIO_MIX], kMaxs[AUDIO_MIX], kDefaults[AUDIO_MIX], plugins::EffectParameterType::FLOAT, 0.01f, "blend", "", false),
    plugins::EffectParameter(kNames[BEAT_GAIN], "Beat Gain", kMins[BEAT_GAIN], kMaxs[BEAT_GAIN], kDefaults[BEAT_GAIN], plugins::EffectParameterType::FLOAT, 0.05f, "audio", "x", false),
    plugins::EffectParameter(kNames[BASS_GAIN], "Bass Gain", kMins[BASS_GAIN], kMaxs[BASS_GAIN], kDefaults[BASS_GAIN], plugins::EffectParameterType::FLOAT, 0.05f, "audio", "x", false),
    plugins::EffectParameter(kNames[MID_GAIN], "Mid Gain", kMins[MID_GAIN], kMaxs[MID_GAIN], kDefaults[MID_GAIN], plugins::EffectParameterType::FLOAT, 0.05f, "audio", "x", false),
    plugins::EffectParameter(kNames[TREBLE_GAIN], "Treble Gain", kMins[TREBLE_GAIN], kMaxs[TREBLE_GAIN], kDefaults[TREBLE_GAIN], plugins::EffectParameterType::FLOAT, 0.05f, "audio", "x", false),
    plugins::EffectParameter(kNames[FLUX_GAIN], "Flux Gain", kMins[FLUX_GAIN], kMaxs[FLUX_GAIN], kDefaults[FLUX_GAIN], plugins::EffectParameterType::FLOAT, 0.05f, "audio", "x", false),
    plugins::EffectParameter(kNames[HARMONIC_GAIN], "Harmonic Gain", kMins[HARMONIC_GAIN], kMaxs[HARMONIC_GAIN], kDefaults[HARMONIC_GAIN], plugins::EffectParameterType::FLOAT, 0.05f, "audio", "x", false),
    plugins::EffectParameter(kNames[RHYTHMIC_GAIN], "Rhythmic Gain", kMins[RHYTHMIC_GAIN], kMaxs[RHYTHMIC_GAIN], kDefaults[RHYTHMIC_GAIN], plugins::EffectParameterType::FLOAT, 0.05f, "audio", "x", false),
    plugins::EffectParameter(kNames[ATTACK_S], "Attack", kMins[ATTACK_S], kMaxs[ATTACK_S], kDefaults[ATTACK_S], plugins::EffectParameterType::FLOAT, 0.01f, "timing", "s", false),
    plugins::EffectParameter(kNames[RELEASE_S], "Release", kMins[RELEASE_S], kMaxs[RELEASE_S], kDefaults[RELEASE_S], plugins::EffectParameterType::FLOAT, 0.05f, "timing", "s", false),
    plugins::EffectParameter(kNames[MOTION_RATE], "Motion Rate", kMins[MOTION_RATE], kMaxs[MOTION_RATE], kDefaults[MOTION_RATE], plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false),
    plugins::EffectParameter(kNames[MOTION_DEPTH], "Motion Depth", kMins[MOTION_DEPTH], kMaxs[MOTION_DEPTH], kDefaults[MOTION_DEPTH], plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false),
    plugins::EffectParameter(kNames[COLOUR_ANCHOR_MIX], "Colour Anchor Mix", kMins[COLOUR_ANCHOR_MIX], kMaxs[COLOUR_ANCHOR_MIX], kDefaults[COLOUR_ANCHOR_MIX], plugins::EffectParameterType::FLOAT, 0.05f, "colour", "", false),
    plugins::EffectParameter(kNames[EVENT_DECAY_S], "Event Decay", kMins[EVENT_DECAY_S], kMaxs[EVENT_DECAY_S], kDefaults[EVENT_DECAY_S], plugins::EffectParameterType::FLOAT, 0.05f, "timing", "s", false),
    plugins::EffectParameter(kNames[MEMORY_GAIN], "Memory Gain", kMins[MEMORY_GAIN], kMaxs[MEMORY_GAIN], kDefaults[MEMORY_GAIN], plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false),
    plugins::EffectParameter(kNames[SILENCE_HOLD], "Silence Hold", kMins[SILENCE_HOLD], kMaxs[SILENCE_HOLD], kDefaults[SILENCE_HOLD], plugins::EffectParameterType::FLOAT, 0.05f, "audio", "", false)
};

static inline float blendAmbientReactive(float ambient, float reactive, const Ar16Controls& c) {
    return lerpf(ambient, reactive, clamp01f(c.audioMix()));
}

static inline ArSignalSnapshot updateSignals(
    ArRuntimeState& state,
    const plugins::EffectContext& ctx,
    const Ar16Controls& controls,
    float dtSignal
) {
    ArSignalSnapshot s{};
    const float attackTau = clampf(controls.attackS(), 0.01f, 1.50f);
    const float releaseTau = clampf(controls.releaseS(), 0.05f, 2.50f);

    const float targetPresence = ctx.audio.available ? 1.0f : controls.silenceHold();
    state.audioPresence = smoothTo(
        state.audioPresence,
        targetPresence,
        dtSignal,
        ctx.audio.available ? attackTau : releaseTau
    );

    const float bassTarget = ctx.audio.available ? clamp01f(ctx.audio.heavyBass()) : fallbackSine(ctx.rawTotalTimeMs, 0.0011f, 0.3f);
    const float midTarget = ctx.audio.available ? clamp01f(ctx.audio.heavyMid()) : fallbackSine(ctx.rawTotalTimeMs, 0.0009f, 1.1f);
    const float trebleTarget = ctx.audio.available ? clamp01f(ctx.audio.heavyTreble()) : fallbackSine(ctx.rawTotalTimeMs, 0.0013f, 2.4f);
    const float fluxTarget = ctx.audio.available ? clamp01f(ctx.audio.fastFlux()) : fallbackTriangle(ctx.rawTotalTimeMs, 0.0008f);

    state.envBass = smoothTo(state.envBass, bassTarget, dtSignal, attackTau);
    state.envMid = smoothTo(state.envMid, midTarget, dtSignal, attackTau);
    state.envTreble = smoothTo(state.envTreble, trebleTarget, dtSignal, attackTau);
    state.envFlux = smoothTo(state.envFlux, fluxTarget, dtSignal, clampf(attackTau * 0.7f, 0.01f, 1.0f));

    s.beatTick = AudioReactivePolicy::audioBeatTick(ctx, 128.0f, state.lastBeatMs);
    if (s.beatTick) state.impactBeat = 1.0f;
    else state.impactBeat = decay(state.impactBeat, dtSignal, controls.eventDecayS());

    const float beatStrength = ctx.audio.available ? clamp01f(ctx.audio.beatStrength()) : fallbackTriangle(ctx.rawTotalTimeMs, 0.0015f);
    s.beat = clamp01f((0.65f * state.impactBeat + 0.35f * beatStrength) * controls.beatGain());
    state.memory = clamp01f(
        decay(state.memory, dtSignal, releaseTau * 1.5f) +
        controls.memoryGain() * (0.18f * s.beat + 0.10f * state.envFlux)
    );

    const uint8_t targetHue = selectMusicalHue(ctx, state.chordGateOpen);
    state.tonalHue = smoothHue(state.tonalHue, static_cast<float>(targetHue), dtSignal, clampf(0.20f + 0.40f * controls.harmonicGain(), 0.15f, 0.90f));

    s.bass = clamp01f(state.envBass * controls.bassGain());
    s.mid = clamp01f(state.envMid * controls.midGain());
    s.treble = clamp01f(state.envTreble * controls.trebleGain());
    s.flux = clamp01f(state.envFlux * controls.fluxGain());
    s.rhythmic = clamp01f((ctx.audio.available ? ctx.audio.rhythmicSaliency() : state.envFlux) * controls.rhythmicGain());
    s.harmonic = clamp01f((ctx.audio.available ? ctx.audio.harmonicSaliency() : state.envMid) * controls.harmonicGain());
    s.rms = clamp01f(ctx.audio.available ? ctx.audio.rms() : (0.35f + 0.65f * state.envBass));
    return s;
}

static inline float tonalBaseHue(const plugins::EffectContext& ctx, const Ar16Controls& controls, const ArRuntimeState& state) {
    return lerpf(24.0f, state.tonalHue, clamp01f(controls.colourAnchorMix())) + static_cast<float>(ctx.gHue) * (1.0f - clamp01f(controls.colourAnchorMix())) * 0.18f;
}

} // namespace lightwaveos::effects::ieffect::lowrisk_ar

