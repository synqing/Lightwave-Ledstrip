/**
 * @file LGPExperimentalAudioPack.cpp
 * @brief 10 experimental audio-reactive effects for LightwaveOS v2
 */

#include "LGPExperimentalAudioPack.h"
#include "ChromaUtils.h"
#include "AudioReactivePolicy.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstdint>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPTransientLatticeEffect
namespace {
constexpr float kLGPTransientLatticeEffectSpeedScale = 1.0f;
constexpr float kLGPTransientLatticeEffectOutputGain = 1.0f;
constexpr float kLGPTransientLatticeEffectCentreBias = 1.0f;

float gLGPTransientLatticeEffectSpeedScale = kLGPTransientLatticeEffectSpeedScale;
float gLGPTransientLatticeEffectOutputGain = kLGPTransientLatticeEffectOutputGain;
float gLGPTransientLatticeEffectCentreBias = kLGPTransientLatticeEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPTransientLatticeEffectParameters[] = {
    {"lgptransient_lattice_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPTransientLatticeEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgptransient_lattice_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPTransientLatticeEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgptransient_lattice_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPTransientLatticeEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPTransientLatticeEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPBassQuakeEffect
namespace {
constexpr float kLGPBassQuakeEffectSpeedScale = 1.0f;
constexpr float kLGPBassQuakeEffectOutputGain = 1.0f;
constexpr float kLGPBassQuakeEffectCentreBias = 1.0f;

float gLGPBassQuakeEffectSpeedScale = kLGPBassQuakeEffectSpeedScale;
float gLGPBassQuakeEffectOutputGain = kLGPBassQuakeEffectOutputGain;
float gLGPBassQuakeEffectCentreBias = kLGPBassQuakeEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPBassQuakeEffectParameters[] = {
    {"lgpbass_quake_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPBassQuakeEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpbass_quake_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPBassQuakeEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpbass_quake_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPBassQuakeEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPBassQuakeEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPSaliencyBloomEffect
namespace {
constexpr float kLGPSaliencyBloomEffectSpeedScale = 1.0f;
constexpr float kLGPSaliencyBloomEffectOutputGain = 1.0f;
constexpr float kLGPSaliencyBloomEffectCentreBias = 1.0f;

float gLGPSaliencyBloomEffectSpeedScale = kLGPSaliencyBloomEffectSpeedScale;
float gLGPSaliencyBloomEffectOutputGain = kLGPSaliencyBloomEffectOutputGain;
float gLGPSaliencyBloomEffectCentreBias = kLGPSaliencyBloomEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPSaliencyBloomEffectParameters[] = {
    {"lgpsaliency_bloom_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPSaliencyBloomEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpsaliency_bloom_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPSaliencyBloomEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpsaliency_bloom_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPSaliencyBloomEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPSaliencyBloomEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPRhythmicGateEffect
namespace {
constexpr float kLGPRhythmicGateEffectSpeedScale = 1.0f;
constexpr float kLGPRhythmicGateEffectOutputGain = 1.0f;
constexpr float kLGPRhythmicGateEffectCentreBias = 1.0f;

float gLGPRhythmicGateEffectSpeedScale = kLGPRhythmicGateEffectSpeedScale;
float gLGPRhythmicGateEffectOutputGain = kLGPRhythmicGateEffectOutputGain;
float gLGPRhythmicGateEffectCentreBias = kLGPRhythmicGateEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPRhythmicGateEffectParameters[] = {
    {"lgprhythmic_gate_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPRhythmicGateEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgprhythmic_gate_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPRhythmicGateEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgprhythmic_gate_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPRhythmicGateEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPRhythmicGateEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPSpectralKnotEffect
namespace {
constexpr float kLGPSpectralKnotEffectSpeedScale = 1.0f;
constexpr float kLGPSpectralKnotEffectOutputGain = 1.0f;
constexpr float kLGPSpectralKnotEffectCentreBias = 1.0f;

float gLGPSpectralKnotEffectSpeedScale = kLGPSpectralKnotEffectSpeedScale;
float gLGPSpectralKnotEffectOutputGain = kLGPSpectralKnotEffectOutputGain;
float gLGPSpectralKnotEffectCentreBias = kLGPSpectralKnotEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPSpectralKnotEffectParameters[] = {
    {"lgpspectral_knot_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPSpectralKnotEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpspectral_knot_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPSpectralKnotEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpspectral_knot_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPSpectralKnotEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPSpectralKnotEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPTrebleNetEffect
namespace {
constexpr float kLGPTrebleNetEffectSpeedScale = 1.0f;
constexpr float kLGPTrebleNetEffectOutputGain = 1.0f;
constexpr float kLGPTrebleNetEffectCentreBias = 1.0f;

float gLGPTrebleNetEffectSpeedScale = kLGPTrebleNetEffectSpeedScale;
float gLGPTrebleNetEffectOutputGain = kLGPTrebleNetEffectOutputGain;
float gLGPTrebleNetEffectCentreBias = kLGPTrebleNetEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPTrebleNetEffectParameters[] = {
    {"lgptreble_net_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPTrebleNetEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgptreble_net_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPTrebleNetEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgptreble_net_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPTrebleNetEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPTrebleNetEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPBeatPrismEffect
namespace {
constexpr float kLGPBeatPrismEffectSpeedScale = 1.0f;
constexpr float kLGPBeatPrismEffectOutputGain = 1.0f;
constexpr float kLGPBeatPrismEffectCentreBias = 1.0f;

float gLGPBeatPrismEffectSpeedScale = kLGPBeatPrismEffectSpeedScale;
float gLGPBeatPrismEffectOutputGain = kLGPBeatPrismEffectOutputGain;
float gLGPBeatPrismEffectCentreBias = kLGPBeatPrismEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPBeatPrismEffectParameters[] = {
    {"lgpbeat_prism_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPBeatPrismEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpbeat_prism_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPBeatPrismEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpbeat_prism_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPBeatPrismEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPBeatPrismEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPFluxRiftEffect
namespace {
constexpr float kLGPFluxRiftEffectSpeedScale = 1.0f;
constexpr float kLGPFluxRiftEffectOutputGain = 1.0f;
constexpr float kLGPFluxRiftEffectCentreBias = 1.0f;

float gLGPFluxRiftEffectSpeedScale = kLGPFluxRiftEffectSpeedScale;
float gLGPFluxRiftEffectOutputGain = kLGPFluxRiftEffectOutputGain;
float gLGPFluxRiftEffectCentreBias = kLGPFluxRiftEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPFluxRiftEffectParameters[] = {
    {"lgpflux_rift_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPFluxRiftEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpflux_rift_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPFluxRiftEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpflux_rift_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPFluxRiftEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPFluxRiftEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPHarmonicTideEffect
namespace {
constexpr float kLGPHarmonicTideEffectSpeedScale = 1.0f;
constexpr float kLGPHarmonicTideEffectOutputGain = 1.0f;
constexpr float kLGPHarmonicTideEffectCentreBias = 1.0f;

float gLGPHarmonicTideEffectSpeedScale = kLGPHarmonicTideEffectSpeedScale;
float gLGPHarmonicTideEffectOutputGain = kLGPHarmonicTideEffectOutputGain;
float gLGPHarmonicTideEffectCentreBias = kLGPHarmonicTideEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPHarmonicTideEffectParameters[] = {
    {"lgpharmonic_tide_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPHarmonicTideEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpharmonic_tide_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPHarmonicTideEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpharmonic_tide_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPHarmonicTideEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPHarmonicTideEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

static constexpr float EX_PI = 3.14159265358979323846f;
static constexpr float EX_TAU = 6.28318530717958647692f;

static constexpr uint8_t NOTE_HUES[12] = {
    0, 12, 24, 40, 56, 74, 92, 112, 134, 154, 178, 202
};

static inline float clamp01f(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline float smoothstep01(float x) {
    const float t = clamp01f(x);
    return t * t * (3.0f - 2.0f * t);
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

static inline float binsRangeEnergy(const plugins::EffectContext& ctx, uint8_t start, uint8_t end) {
    if (!ctx.audio.available || start > end) return 0.0f;
    float sum = 0.0f;
    uint8_t count = 0;
    for (uint8_t i = start; i <= end && i < 64; ++i) {
        sum += clamp01f(ctx.audio.binAdaptive(i));
        ++count;
    }
    return (count > 0) ? (sum / static_cast<float>(count)) : 0.0f;
}

static inline uint8_t dominantNoteFromBins(const plugins::EffectContext& ctx) {
    if (!ctx.audio.available) return 0;

    // Accumulate per-note scores across octaves, then use circular weighted
    // mean to avoid discontinuous jumps when two adjacent notes compete.
    float scores[12] = {};
    for (uint8_t note = 0; note < 12; ++note) {
        for (uint8_t b = note; b < 48; b = static_cast<uint8_t>(b + 12)) {
            scores[note] += clamp01f(ctx.audio.binAdaptive(b));
        }
    }

    // Circular weighted mean over 12 note positions (30-degree steps)
    float cx = 0.0f, sy = 0.0f;
    for (uint8_t i = 0; i < 12; ++i) {
        cx += scores[i] * effects::chroma::kCos[i];
        sy += scores[i] * effects::chroma::kSin[i];
    }
    float angle = atan2f(sy, cx);
    if (angle < 0.0f) angle += EX_TAU;

    // Map angle back to nearest note index (0-11)
    uint8_t note = static_cast<uint8_t>(roundf(angle * (12.0f / EX_TAU))) % 12;
    return note;
}

static inline uint8_t selectMusicalHue(const plugins::EffectContext& ctx, bool& chordGateOpen) {
    if (!ctx.audio.available) return 24;

    // Schmitt trigger hysteresis on chord confidence (enter 0.40, exit 0.25)
    // prevents rapid switching between chord root and bin-derived note.
    const float conf = ctx.audio.chordConfidence();
    if (conf >= 0.40f) chordGateOpen = true;
    else if (conf <= 0.25f) chordGateOpen = false;

    const uint8_t note = chordGateOpen
        ? static_cast<uint8_t>(ctx.audio.rootNote() % 12)
        : dominantNoteFromBins(ctx);
    return NOTE_HUES[note];
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

// Circular smoothing for note index domain [0, 12).
// Same shortest-arc approach as smoothHue but with period 12.
static inline float smoothNoteCircular(float current, float target, float dt, float tauS) {
    float delta = target - current;
    while (delta > 6.0f) delta -= 12.0f;
    while (delta < -6.0f) delta += 12.0f;
    const float next = current + delta * expAlpha(dt, tauS);
    float wrapped = fmodf(next, 12.0f);
    if (wrapped < 0.0f) wrapped += 12.0f;
    return wrapped;
}

static inline uint8_t toBrightness(float intensity, float master) {
    return static_cast<uint8_t>(255.0f * clamp01f(intensity) * clamp01f(master));
}

static inline void setCentrePairDual(
    plugins::EffectContext& ctx,
    uint16_t dist,
    const CRGB& strip1Color,
    const CRGB& strip2Color
) {
    const uint16_t left1 = CENTER_LEFT - dist;
    const uint16_t right1 = CENTER_RIGHT + dist;
    const uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
    const uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;

    if (left1 < ctx.ledCount) ctx.leds[left1] = strip1Color;
    if (right1 < ctx.ledCount) ctx.leds[right1] = strip1Color;
    if (left2 < ctx.ledCount) ctx.leds[left2] = strip2Color;
    if (right2 < ctx.ledCount) ctx.leds[right2] = strip2Color;
}

static inline void setCentrePairMono(
    plugins::EffectContext& ctx,
    uint16_t dist,
    const CRGB& color
) {
    setCentrePairDual(ctx, dist, color, color);
}

static inline float fallbackSine(uint32_t rawMs, float rate, float phaseOffset = 0.0f) {
    return 0.5f + 0.5f * sinf(static_cast<float>(rawMs) * rate + phaseOffset);
}

static inline float trackAudioPresence(float current, bool audioAvailable, float dtSignal) {
    const float tau = audioAvailable ? 0.06f : 0.32f;
    return smoothTo(current, audioAvailable ? 1.0f : 0.0f, dtSignal, tau);
}

} // namespace

// ---------------------------------------------------------------------------
// LGP Flux Rift
// Principle: travelling phase-dislocation seam with beat shock release.
// ---------------------------------------------------------------------------

LGPFluxRiftEffect::LGPFluxRiftEffect() = default;

bool LGPFluxRiftEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPFluxRiftEffect
    gLGPFluxRiftEffectSpeedScale = kLGPFluxRiftEffectSpeedScale;
    gLGPFluxRiftEffectOutputGain = kLGPFluxRiftEffectOutputGain;
    gLGPFluxRiftEffectCentreBias = kLGPFluxRiftEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPFluxRiftEffect

    m_phase = 0.0f;
    m_fluxEnv = 0.0f;
    m_beatPulse = 0.0f;
    m_lastBeatMs = 0;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPFluxRiftEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    const float fluxTarget = ctx.audio.available
        ? clamp01f(0.70f * ctx.audio.fastFlux() + 0.30f * ctx.audio.overallSaliency())
        : fallbackSine(ctx.rawTotalTimeMs, 0.0013f, 0.7f);
    m_fluxEnv = smoothTo(m_fluxEnv, fluxTarget, dtSignal, 0.10f);

    const bool beatTick = AudioReactivePolicy::audioBeatTick(ctx, 128.0f, m_lastBeatMs);
    if (beatTick) {
        m_beatPulse = 1.0f;
    } else {
        m_beatPulse = decay(m_beatPulse, dtSignal, 0.25f);
    }

    m_phase += 0.85f * (0.55f + 1.20f * m_fluxEnv) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float seamPos = clamp01f(1.0f - m_beatPulse);
    const float hueTarget = static_cast<float>(selectMusicalHue(ctx, m_chordGateOpen));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);
        const float seam = tanhf((d - seamPos) * (8.0f + 16.0f * m_fluxEnv));
        const float carrierA = sinf(static_cast<float>(dist) * 0.22f - m_phase * 3.5f);
        const float carrierB = sinf(static_cast<float>(dist) * 0.09f + m_phase * 5.1f);
        const float dislocation = 0.5f + 0.5f * tanhf(-1.35f * seam + 0.65f * carrierA + 0.35f * carrierB);
        const float shock = expf(-fabsf(d - seamPos) * 16.0f) * m_beatPulse;
        const float intensity = clamp01f(dislocation * (0.35f + 0.65f * m_fluxEnv) + 0.9f * shock);

        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue + static_cast<uint8_t>(d * 48.0f) + static_cast<uint8_t>(m_fluxEnv * 22.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPFluxRiftEffect
uint8_t LGPFluxRiftEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPFluxRiftEffectParameters) / sizeof(kLGPFluxRiftEffectParameters[0]));
}

const plugins::EffectParameter* LGPFluxRiftEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPFluxRiftEffectParameters[index];
}

bool LGPFluxRiftEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpflux_rift_effect_speed_scale") == 0) {
        gLGPFluxRiftEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpflux_rift_effect_output_gain") == 0) {
        gLGPFluxRiftEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpflux_rift_effect_centre_bias") == 0) {
        gLGPFluxRiftEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPFluxRiftEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpflux_rift_effect_speed_scale") == 0) return gLGPFluxRiftEffectSpeedScale;
    if (strcmp(name, "lgpflux_rift_effect_output_gain") == 0) return gLGPFluxRiftEffectOutputGain;
    if (strcmp(name, "lgpflux_rift_effect_centre_bias") == 0) return gLGPFluxRiftEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPFluxRiftEffect

void LGPFluxRiftEffect::cleanup() {}

const plugins::EffectMetadata& LGPFluxRiftEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Flux Rift",
        "Transient flux opens a travelling centre-out rift",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Beat Prism
// Principle: beat-launched prism fronts refracting through radial spokes.
// ---------------------------------------------------------------------------

LGPBeatPrismEffect::LGPBeatPrismEffect() = default;

bool LGPBeatPrismEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPBeatPrismEffect
    gLGPBeatPrismEffectSpeedScale = kLGPBeatPrismEffectSpeedScale;
    gLGPBeatPrismEffectOutputGain = kLGPBeatPrismEffectOutputGain;
    gLGPBeatPrismEffectCentreBias = kLGPBeatPrismEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPBeatPrismEffect

    m_phase = 0.0f;
    m_prism = 0.0f;
    m_beatPulse = 0.0f;
    m_lastBeatMs = 0;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPBeatPrismEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    const float treble = binsRangeEnergy(ctx, 42, 63);
    const float prismTarget = ctx.audio.available
        ? clamp01f(0.55f * ctx.audio.beatStrength() + 0.45f * treble)
        : fallbackSine(ctx.rawTotalTimeMs, 0.0011f, 1.0f);
    m_prism = smoothTo(m_prism, prismTarget, dtSignal, 0.08f);

    const bool beatTick = AudioReactivePolicy::audioBeatTick(ctx, 128.0f, m_lastBeatMs);
    if (beatTick) {
        m_beatPulse = 1.0f;
    } else {
        m_beatPulse = decay(m_beatPulse, dtSignal, 0.20f);
    }

    m_phase += 0.90f * (0.55f + 1.35f * m_prism) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float frontPos = clamp01f(1.0f - m_beatPulse);
    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 8));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);
        const float spokes = fabsf(sinf((d * (5.5f + 13.0f * m_prism) - m_phase * 0.7f) * EX_PI));
        const float facets = 0.5f + 0.5f * cosf((d * 3.5f + m_phase * 0.35f) * EX_TAU);
        const float refract = 0.5f + 0.5f * sinf(d * (2.2f + 4.0f * m_prism) * EX_TAU - m_phase * 1.35f);
        const float front = expf(-fabsf(d - frontPos) * (8.0f + 10.0f * m_prism)) * m_beatPulse;
        const float intensity = clamp01f((0.20f + 0.80f * spokes) * (0.25f + 0.75f * facets) *
                                         (0.20f + 0.80f * refract) + front * 0.95f);

        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue + static_cast<uint8_t>(spokes * 32.0f) + static_cast<uint8_t>(d * 28.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPBeatPrismEffect
uint8_t LGPBeatPrismEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPBeatPrismEffectParameters) / sizeof(kLGPBeatPrismEffectParameters[0]));
}

const plugins::EffectParameter* LGPBeatPrismEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPBeatPrismEffectParameters[index];
}

bool LGPBeatPrismEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpbeat_prism_effect_speed_scale") == 0) {
        gLGPBeatPrismEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpbeat_prism_effect_output_gain") == 0) {
        gLGPBeatPrismEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpbeat_prism_effect_centre_bias") == 0) {
        gLGPBeatPrismEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPBeatPrismEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpbeat_prism_effect_speed_scale") == 0) return gLGPBeatPrismEffectSpeedScale;
    if (strcmp(name, "lgpbeat_prism_effect_output_gain") == 0) return gLGPBeatPrismEffectOutputGain;
    if (strcmp(name, "lgpbeat_prism_effect_centre_bias") == 0) return gLGPBeatPrismEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPBeatPrismEffect

void LGPBeatPrismEffect::cleanup() {}

const plugins::EffectMetadata& LGPBeatPrismEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Beat Prism",
        "Beat-front prism rays with edgeward pressure travel",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Harmonic Tide
// Principle: triadic standing-tide superposition anchored to harmonic state.
// ---------------------------------------------------------------------------

LGPHarmonicTideEffect::LGPHarmonicTideEffect() = default;

bool LGPHarmonicTideEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPHarmonicTideEffect
    gLGPHarmonicTideEffectSpeedScale = kLGPHarmonicTideEffectSpeedScale;
    gLGPHarmonicTideEffectOutputGain = kLGPHarmonicTideEffectOutputGain;
    gLGPHarmonicTideEffectCentreBias = kLGPHarmonicTideEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPHarmonicTideEffect

    m_phase = 0.0f;
    m_harmonic = 0.0f;
    m_rootSmooth = 0.0f;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPHarmonicTideEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    const float harmonicTarget = ctx.audio.available
        ? clamp01f(fmaxf(ctx.audio.harmonicSaliency(), ctx.audio.chordConfidence()))
        : fallbackSine(ctx.rawTotalTimeMs, 0.0008f, 1.6f);
    m_harmonic = smoothTo(m_harmonic, harmonicTarget, dtSignal, 0.20f);

    // Hysteresis gate for chord confidence (enter 0.40, exit 0.25)
    if (ctx.audio.available) {
        const float conf = ctx.audio.chordConfidence();
        if (conf >= 0.40f) m_chordGateOpen = true;
        else if (conf <= 0.25f) m_chordGateOpen = false;
    }

    // Root note with hysteresis gate and circular smoothing (note domain wraps at 12)
    const float rootTarget = ctx.audio.available
        ? static_cast<float>(m_chordGateOpen ? (ctx.audio.rootNote() % 12)
                                             : dominantNoteFromBins(ctx))
        : 2.0f;
    m_rootSmooth = smoothNoteCircular(m_rootSmooth, rootTarget, dtSignal, 0.30f);

    const float mid = ctx.audio.available ? clamp01f(ctx.audio.heavyMid()) : 0.25f;
    m_phase += 0.75f * (0.65f + 1.15f * mid) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const uint8_t rootBin = static_cast<uint8_t>(roundf(m_rootSmooth)) % 12;
    const bool minor = ctx.audio.available ? ctx.audio.isMinor() : false;
    const uint8_t thirdBin = static_cast<uint8_t>((rootBin + (minor ? 3 : 4)) % 12);
    const uint8_t fifthBin = static_cast<uint8_t>((rootBin + 7) % 12);

    const uint8_t binStep = static_cast<uint8_t>(255.0f / 12.0f);
    const uint8_t hueRoot = static_cast<uint8_t>(ctx.gHue + rootBin * binStep);
    const uint8_t hueThird = static_cast<uint8_t>(ctx.gHue + thirdBin * binStep);
    const uint8_t hueFifth = static_cast<uint8_t>(ctx.gHue + fifthBin * binStep);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);

        const float outward = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.09f - m_phase * 3.8f);
        const float inward = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.07f + m_phase * 2.7f);
        const float standing = fabsf(sinf(static_cast<float>(dist) * 0.043f + m_phase * 1.1f));
        const float envelope = (0.28f + 0.72f * m_harmonic) * (0.30f + 0.70f * expf(-d * 2.0f));
        const float intensity = clamp01f((0.45f * outward + 0.35f * inward + 0.20f * standing) * envelope);

        const uint8_t brightness = toBrightness(intensity, master);
        const uint8_t paletteIndex = static_cast<uint8_t>(dist + static_cast<uint8_t>(standing * 38.0f));

        float wRoot = clamp01f(1.20f - 1.55f * d);
        float wFifth = clamp01f(0.30f + 1.00f * d);
        float wThird = m_harmonic * clamp01f(1.0f - fabsf(d - 0.35f) * 3.1f);
        const float wSum = wRoot + wThird + wFifth;
        if (wSum > 0.0001f) {
            wRoot /= wSum;
            wThird /= wSum;
            wFifth /= wSum;
        }

        const uint8_t bRoot = static_cast<uint8_t>(brightness * wRoot);
        const uint8_t bThird = static_cast<uint8_t>(brightness * wThird);
        const uint8_t bFifth = static_cast<uint8_t>(brightness * wFifth);

        CRGB c1 = CRGB::Black;
        if (bRoot) {
            c1 += ctx.palette.getColor(static_cast<uint8_t>(hueRoot + paletteIndex), bRoot);
        }
        if (bThird) {
            c1 += ctx.palette.getColor(static_cast<uint8_t>(hueThird + paletteIndex), bThird);
        }
        if (bFifth) {
            c1 += ctx.palette.getColor(static_cast<uint8_t>(hueFifth + paletteIndex), bFifth);
        }
        setCentrePairMono(ctx, dist, c1);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPHarmonicTideEffect
uint8_t LGPHarmonicTideEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPHarmonicTideEffectParameters) / sizeof(kLGPHarmonicTideEffectParameters[0]));
}

const plugins::EffectParameter* LGPHarmonicTideEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPHarmonicTideEffectParameters[index];
}

bool LGPHarmonicTideEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpharmonic_tide_effect_speed_scale") == 0) {
        gLGPHarmonicTideEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpharmonic_tide_effect_output_gain") == 0) {
        gLGPHarmonicTideEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpharmonic_tide_effect_centre_bias") == 0) {
        gLGPHarmonicTideEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPHarmonicTideEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpharmonic_tide_effect_speed_scale") == 0) return gLGPHarmonicTideEffectSpeedScale;
    if (strcmp(name, "lgpharmonic_tide_effect_output_gain") == 0) return gLGPHarmonicTideEffectOutputGain;
    if (strcmp(name, "lgpharmonic_tide_effect_centre_bias") == 0) return gLGPHarmonicTideEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPHarmonicTideEffect

void LGPHarmonicTideEffect::cleanup() {}

const plugins::EffectMetadata& LGPHarmonicTideEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Harmonic Tide",
        "Chord-anchored tidal bands with centre-held continuity",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Bass Quake
// Principle: non-linear compression field with outward shock cells.
// ---------------------------------------------------------------------------

LGPBassQuakeEffect::LGPBassQuakeEffect() = default;

bool LGPBassQuakeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPBassQuakeEffect
    gLGPBassQuakeEffectSpeedScale = kLGPBassQuakeEffectSpeedScale;
    gLGPBassQuakeEffectOutputGain = kLGPBassQuakeEffectOutputGain;
    gLGPBassQuakeEffectCentreBias = kLGPBassQuakeEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPBassQuakeEffect

    m_phase = 0.0f;
    m_bassEnv = 0.0f;
    m_impact = 0.0f;
    m_lastBeatMs = 0;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPBassQuakeEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    const float bassTarget = ctx.audio.available
        ? clamp01f(ctx.audio.heavyBass())
        : fallbackSine(ctx.rawTotalTimeMs, 0.0011f, 2.2f);
    m_bassEnv = smoothTo(m_bassEnv, bassTarget, dtSignal, 0.06f);

    const bool beatTick = AudioReactivePolicy::audioBeatTick(ctx, 128.0f, m_lastBeatMs);
    const float seed = clamp01f(0.80f * m_bassEnv + (beatTick ? 0.45f : 0.0f));
    if (seed > m_impact) {
        m_impact = seed;
    } else {
        m_impact = decay(m_impact, dtSignal, 0.22f);
    }

    m_phase += 0.80f * (0.45f + 1.75f * m_bassEnv) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float shockPos = clamp01f(1.0f - m_impact);
    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 10));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);
        const float compression = powf(clamp01f(1.0f - d), 0.55f + 2.30f * (1.0f - m_bassEnv));
        const float cell = 0.5f + 0.5f * sinf(static_cast<float>(dist) * (0.18f + 0.22f * m_bassEnv) - m_phase * 4.2f);
        const float overtone = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.47f - m_phase * 7.8f);
        const float shock = expf(-fabsf(d - shockPos) * (10.0f + 13.0f * m_impact)) * m_impact;
        const float intensity = clamp01f((0.55f * compression + 0.45f * cell) * (0.35f + 0.65f * overtone) + 0.95f * shock);

        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue + static_cast<uint8_t>(shock * 30.0f) + static_cast<uint8_t>(d * 22.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPBassQuakeEffect
uint8_t LGPBassQuakeEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPBassQuakeEffectParameters) / sizeof(kLGPBassQuakeEffectParameters[0]));
}

const plugins::EffectParameter* LGPBassQuakeEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPBassQuakeEffectParameters[index];
}

bool LGPBassQuakeEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpbass_quake_effect_speed_scale") == 0) {
        gLGPBassQuakeEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpbass_quake_effect_output_gain") == 0) {
        gLGPBassQuakeEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpbass_quake_effect_centre_bias") == 0) {
        gLGPBassQuakeEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPBassQuakeEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpbass_quake_effect_speed_scale") == 0) return gLGPBassQuakeEffectSpeedScale;
    if (strcmp(name, "lgpbass_quake_effect_output_gain") == 0) return gLGPBassQuakeEffectOutputGain;
    if (strcmp(name, "lgpbass_quake_effect_centre_bias") == 0) return gLGPBassQuakeEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPBassQuakeEffect

void LGPBassQuakeEffect::cleanup() {}

const plugins::EffectMetadata& LGPBassQuakeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Bass Quake",
        "Sub-bass compression waves with outward shock release",
        plugins::EffectCategory::SHOCKWAVE,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Treble Net
// Principle: moire-like high-frequency lattice with edge filament shimmer.
// ---------------------------------------------------------------------------

LGPTrebleNetEffect::LGPTrebleNetEffect() = default;

bool LGPTrebleNetEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPTrebleNetEffect
    gLGPTrebleNetEffectSpeedScale = kLGPTrebleNetEffectSpeedScale;
    gLGPTrebleNetEffectOutputGain = kLGPTrebleNetEffectOutputGain;
    gLGPTrebleNetEffectCentreBias = kLGPTrebleNetEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPTrebleNetEffect

    m_phase = 0.0f;
    m_trebleEnv = 0.0f;
    m_shimmer = 0.0f;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPTrebleNetEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    const float trebleTarget = ctx.audio.available
        ? clamp01f(0.65f * ctx.audio.heavyTreble() + 0.35f * ctx.audio.timbralSaliency())
        : fallbackSine(ctx.rawTotalTimeMs, 0.0016f, 1.7f);
    m_trebleEnv = smoothTo(m_trebleEnv, trebleTarget, dtSignal, 0.09f);

    const bool shimmerHit = ctx.audio.available && (ctx.audio.isHihatHit() || ctx.audio.timbralSaliency() > 0.55f);
    if (shimmerHit) {
        m_shimmer = 1.0f;
    } else {
        m_shimmer = decay(m_shimmer, dtSignal, 0.14f);
    }

    m_phase += 0.95f * (0.45f + 1.55f * m_trebleEnv) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 116));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);

        const float netA = sinf(static_cast<float>(dist) * (0.28f + 0.14f * m_trebleEnv) - m_phase * 4.7f);
        const float netB = sinf(static_cast<float>(dist) * (0.52f + 0.10f * m_trebleEnv) + m_phase * 6.1f);
        const float moire = 1.0f - fabsf(netA * netB);
        const float edge = smoothstep01(d);

        const float spark = powf(0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.9f + m_phase * 12.0f), 6.0f) * m_shimmer;
        const float intensity = clamp01f((0.20f + 0.80f * moire) * (0.25f + 0.75f * edge) *
                                         (0.30f + 0.70f * m_trebleEnv) + spark);

        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue + static_cast<uint8_t>(moire * 28.0f) + static_cast<uint8_t>(edge * 16.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPTrebleNetEffect
uint8_t LGPTrebleNetEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPTrebleNetEffectParameters) / sizeof(kLGPTrebleNetEffectParameters[0]));
}

const plugins::EffectParameter* LGPTrebleNetEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPTrebleNetEffectParameters[index];
}

bool LGPTrebleNetEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgptreble_net_effect_speed_scale") == 0) {
        gLGPTrebleNetEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgptreble_net_effect_output_gain") == 0) {
        gLGPTrebleNetEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgptreble_net_effect_centre_bias") == 0) {
        gLGPTrebleNetEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPTrebleNetEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgptreble_net_effect_speed_scale") == 0) return gLGPTrebleNetEffectSpeedScale;
    if (strcmp(name, "lgptreble_net_effect_output_gain") == 0) return gLGPTrebleNetEffectOutputGain;
    if (strcmp(name, "lgptreble_net_effect_centre_bias") == 0) return gLGPTrebleNetEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPTrebleNetEffect

void LGPTrebleNetEffect::cleanup() {}

const plugins::EffectMetadata& LGPTrebleNetEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Treble Net",
        "Timbral shimmer lattice with edge-biased spectral filaments",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Rhythmic Gate
// Principle: temporal shutters and travelling seam locked to beat policy.
// ---------------------------------------------------------------------------

LGPRhythmicGateEffect::LGPRhythmicGateEffect() = default;

bool LGPRhythmicGateEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPRhythmicGateEffect
    gLGPRhythmicGateEffectSpeedScale = kLGPRhythmicGateEffectSpeedScale;
    gLGPRhythmicGateEffectOutputGain = kLGPRhythmicGateEffectOutputGain;
    gLGPRhythmicGateEffectCentreBias = kLGPRhythmicGateEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPRhythmicGateEffect

    m_phase = 0.0f;
    m_gate = 0.0f;
    m_pulse = 0.0f;
    m_lastBeatMs = 0;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPRhythmicGateEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    const float rhythmicTarget = ctx.audio.available
        ? clamp01f(ctx.audio.rhythmicSaliency())
        : fallbackSine(ctx.rawTotalTimeMs, 0.0010f, 0.5f);
    m_gate = smoothTo(m_gate, rhythmicTarget, dtSignal, 0.11f);

    const bool beatTick = AudioReactivePolicy::audioBeatTick(ctx, 128.0f, m_lastBeatMs);
    if (beatTick) {
        m_pulse = 1.0f;
    } else {
        m_pulse = decay(m_pulse, dtSignal, 0.17f);
    }

    m_phase += 0.85f * (0.60f + 1.10f * m_gate) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float gateRate = 0.0013f + 0.0034f * (0.25f + 0.75f * m_gate);
    const float gateClock = fmodf(static_cast<float>(ctx.rawTotalTimeMs) * gateRate, 1.0f);
    const float duty = 0.24f + 0.48f * m_gate;
    const float frontPos = clamp01f(1.0f - m_pulse);

    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 30));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);

        const float bars = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.16f - m_phase * 3.8f);
        const float gateRaw = duty - gateClock;
        const float gateSoft = clamp01f(0.5f + gateRaw / (0.08f + 0.20f * (1.0f - m_gate)));
        const float gateMask = smoothstep01(gateSoft);
        const float seam = expf(-fabsf(d - frontPos) * 13.5f) * m_pulse;

        const float intensity = clamp01f((0.18f + 0.82f * gateMask) * (0.25f + 0.75f * bars) *
                                         (0.32f + 0.68f * m_gate) + seam * 0.85f);

        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue + static_cast<uint8_t>(gateMask * 26.0f) + static_cast<uint8_t>(bars * 18.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPRhythmicGateEffect
uint8_t LGPRhythmicGateEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPRhythmicGateEffectParameters) / sizeof(kLGPRhythmicGateEffectParameters[0]));
}

const plugins::EffectParameter* LGPRhythmicGateEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPRhythmicGateEffectParameters[index];
}

bool LGPRhythmicGateEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgprhythmic_gate_effect_speed_scale") == 0) {
        gLGPRhythmicGateEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgprhythmic_gate_effect_output_gain") == 0) {
        gLGPRhythmicGateEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgprhythmic_gate_effect_centre_bias") == 0) {
        gLGPRhythmicGateEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPRhythmicGateEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgprhythmic_gate_effect_speed_scale") == 0) return gLGPRhythmicGateEffectSpeedScale;
    if (strcmp(name, "lgprhythmic_gate_effect_output_gain") == 0) return gLGPRhythmicGateEffectOutputGain;
    if (strcmp(name, "lgprhythmic_gate_effect_centre_bias") == 0) return gLGPRhythmicGateEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPRhythmicGateEffect

void LGPRhythmicGateEffect::cleanup() {}

const plugins::EffectMetadata& LGPRhythmicGateEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Rhythmic Gate",
        "Beat-gated lattice shutters with travelling pulse seams",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Spectral Knot
// Principle: coupled counter-propagating knot rings from spectral imbalance.
// ---------------------------------------------------------------------------

LGPSpectralKnotEffect::LGPSpectralKnotEffect() = default;

bool LGPSpectralKnotEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPSpectralKnotEffect
    gLGPSpectralKnotEffectSpeedScale = kLGPSpectralKnotEffectSpeedScale;
    gLGPSpectralKnotEffectOutputGain = kLGPSpectralKnotEffectOutputGain;
    gLGPSpectralKnotEffectCentreBias = kLGPSpectralKnotEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPSpectralKnotEffect

    m_phase = 0.0f;
    m_knot = 0.0f;
    m_rotation = 0.0f;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPSpectralKnotEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    const float low = binsRangeEnergy(ctx, 0, 10);
    const float mid = binsRangeEnergy(ctx, 12, 32);
    const float high = binsRangeEnergy(ctx, 36, 63);

    const float knotTarget = ctx.audio.available
        ? clamp01f(fabsf(low - high) + 0.45f * mid)
        : fallbackSine(ctx.rawTotalTimeMs, 0.0012f, 2.4f);
    m_knot = smoothTo(m_knot, knotTarget, dtSignal, 0.14f);

    m_rotation += 0.60f * (0.35f + 0.95f * m_knot) * dtVisual;
    if (m_rotation > EX_TAU) m_rotation -= EX_TAU;

    m_phase += 0.78f * (0.60f + 1.20f * mid) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float knotPos = clamp01f(0.5f + 0.28f * sinf(m_rotation));
    const float antiPos = 1.0f - knotPos;
    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 44));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);

        const float ringA = expf(-fabsf(d - knotPos) * (8.5f + 7.0f * m_knot));
        const float ringB = expf(-fabsf(d - antiPos) * (8.5f + 7.0f * m_knot));
        const float braid1 = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.23f - m_phase * 4.6f);
        const float braid2 = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.37f + m_phase * 5.2f);
        const float weave = fabsf(braid1 - braid2);

        const float intensity = clamp01f(fmaxf(ringA, ringB) * (0.25f + 0.75f * weave) * (0.30f + 0.70f * m_knot));

        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue + static_cast<uint8_t>(weave * 40.0f) + static_cast<uint8_t>(knotPos * 18.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPSpectralKnotEffect
uint8_t LGPSpectralKnotEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPSpectralKnotEffectParameters) / sizeof(kLGPSpectralKnotEffectParameters[0]));
}

const plugins::EffectParameter* LGPSpectralKnotEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPSpectralKnotEffectParameters[index];
}

bool LGPSpectralKnotEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpspectral_knot_effect_speed_scale") == 0) {
        gLGPSpectralKnotEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpspectral_knot_effect_output_gain") == 0) {
        gLGPSpectralKnotEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpspectral_knot_effect_centre_bias") == 0) {
        gLGPSpectralKnotEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPSpectralKnotEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpspectral_knot_effect_speed_scale") == 0) return gLGPSpectralKnotEffectSpeedScale;
    if (strcmp(name, "lgpspectral_knot_effect_output_gain") == 0) return gLGPSpectralKnotEffectOutputGain;
    if (strcmp(name, "lgpspectral_knot_effect_centre_bias") == 0) return gLGPSpectralKnotEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPSpectralKnotEffect

void LGPSpectralKnotEffect::cleanup() {}

const plugins::EffectMetadata& LGPSpectralKnotEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Spectral Knot",
        "Frequency-balance knot fields crossing in mirrored rings",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Saliency Bloom
// Principle: activator-inhibitor bloom shell riding over centre diffusion bed.
// ---------------------------------------------------------------------------

LGPSaliencyBloomEffect::LGPSaliencyBloomEffect() = default;

bool LGPSaliencyBloomEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPSaliencyBloomEffect
    gLGPSaliencyBloomEffectSpeedScale = kLGPSaliencyBloomEffectSpeedScale;
    gLGPSaliencyBloomEffectOutputGain = kLGPSaliencyBloomEffectOutputGain;
    gLGPSaliencyBloomEffectCentreBias = kLGPSaliencyBloomEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPSaliencyBloomEffect

    m_phase = 0.0f;
    m_saliency = 0.0f;
    m_bloom = 0.0f;
    m_lastBeatMs = 0;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPSaliencyBloomEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    const float saliencyTarget = ctx.audio.available
        ? clamp01f(ctx.audio.overallSaliency())
        : fallbackSine(ctx.rawTotalTimeMs, 0.0009f, 1.2f);
    m_saliency = smoothTo(m_saliency, saliencyTarget, dtSignal, 0.16f);

    const bool beatTick = AudioReactivePolicy::audioBeatTick(ctx, 128.0f, m_lastBeatMs);
    if (beatTick) {
        m_bloom = 1.0f;
    } else {
        const float floor = clamp01f(0.25f * m_saliency);
        m_bloom = fmaxf(decay(m_bloom, dtSignal, 0.42f), floor);
    }

    m_phase += 0.68f * (0.45f + 1.10f * m_saliency) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float ringPos = clamp01f(1.0f - m_bloom);
    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 14));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);

        const float bed = expf(-d * (1.8f + 1.3f * (1.0f - m_saliency))) * (0.35f + 0.65f * (0.5f + 0.5f * sinf(m_phase * 2.4f)));
        const float activator = expf(-fabsf(d - ringPos) * 11.0f) * m_bloom;
        const float inhibitor = expf(-fabsf(d - clamp01f(ringPos + 0.10f)) * 14.5f) * m_bloom;
        const float intensity = clamp01f(bed * (0.35f + 0.65f * m_saliency) + activator * 0.95f - inhibitor * 0.50f);

        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue + static_cast<uint8_t>(activator * 30.0f) + static_cast<uint8_t>(d * 22.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPSaliencyBloomEffect
uint8_t LGPSaliencyBloomEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPSaliencyBloomEffectParameters) / sizeof(kLGPSaliencyBloomEffectParameters[0]));
}

const plugins::EffectParameter* LGPSaliencyBloomEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPSaliencyBloomEffectParameters[index];
}

bool LGPSaliencyBloomEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpsaliency_bloom_effect_speed_scale") == 0) {
        gLGPSaliencyBloomEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpsaliency_bloom_effect_output_gain") == 0) {
        gLGPSaliencyBloomEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpsaliency_bloom_effect_centre_bias") == 0) {
        gLGPSaliencyBloomEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPSaliencyBloomEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpsaliency_bloom_effect_speed_scale") == 0) return gLGPSaliencyBloomEffectSpeedScale;
    if (strcmp(name, "lgpsaliency_bloom_effect_output_gain") == 0) return gLGPSaliencyBloomEffectOutputGain;
    if (strcmp(name, "lgpsaliency_bloom_effect_centre_bias") == 0) return gLGPSaliencyBloomEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPSaliencyBloomEffect

void LGPSaliencyBloomEffect::cleanup() {}

const plugins::EffectMetadata& LGPSaliencyBloomEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Saliency Bloom",
        "Overall novelty drives expanding bloom radius and density",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Transient Lattice
// Principle: transient impacts etch and decay a centre-symmetric scaffold.
// ---------------------------------------------------------------------------

LGPTransientLatticeEffect::LGPTransientLatticeEffect() = default;

bool LGPTransientLatticeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPTransientLatticeEffect
    gLGPTransientLatticeEffectSpeedScale = kLGPTransientLatticeEffectSpeedScale;
    gLGPTransientLatticeEffectOutputGain = kLGPTransientLatticeEffectOutputGain;
    gLGPTransientLatticeEffectCentreBias = kLGPTransientLatticeEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPTransientLatticeEffect

    m_phase = 0.0f;
    m_transient = 0.0f;
    m_memory = 0.0f;
    m_lastBeatMs = 0;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPTransientLatticeEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    float seed = ctx.audio.available ? clamp01f(ctx.audio.fastFlux()) : fallbackSine(ctx.rawTotalTimeMs, 0.0015f, 2.0f);
    if (ctx.audio.available && ctx.audio.isSnareHit()) seed = fmaxf(seed, 0.95f);
    if (ctx.audio.available && ctx.audio.isHihatHit()) seed = fmaxf(seed, 0.70f);
    if (AudioReactivePolicy::audioBeatTick(ctx, 128.0f, m_lastBeatMs)) seed = fmaxf(seed, 0.82f);

    if (seed > m_transient) {
        m_transient = seed;
    } else {
        m_transient = decay(m_transient, dtSignal, 0.19f);
    }

    m_memory = clamp01f(m_memory * expf(-dtSignal / 0.68f) + m_transient * 0.20f);

    m_phase += 0.92f * (0.55f + 1.40f * m_transient) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float ringPos = clamp01f(1.0f - m_transient);
    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 62));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);

        const float l1 = fabsf(sinf(static_cast<float>(dist) * 0.27f - m_phase * 4.2f));
        const float l2 = fabsf(sinf(static_cast<float>(dist) * 0.14f + m_phase * 6.8f));
        const float scaffold = l1 * l2;
        const float impact = expf(-fabsf(d - ringPos) * 13.0f) * m_transient;
        const float afterglow = expf(-d * 2.4f) * m_memory;
        const float intensity = clamp01f(scaffold * (0.20f + 0.80f * m_memory) + 0.92f * impact + 0.35f * afterglow);

        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue + static_cast<uint8_t>(scaffold * 44.0f) + static_cast<uint8_t>(m_memory * 12.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPTransientLatticeEffect
uint8_t LGPTransientLatticeEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPTransientLatticeEffectParameters) / sizeof(kLGPTransientLatticeEffectParameters[0]));
}

const plugins::EffectParameter* LGPTransientLatticeEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPTransientLatticeEffectParameters[index];
}

bool LGPTransientLatticeEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgptransient_lattice_effect_speed_scale") == 0) {
        gLGPTransientLatticeEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgptransient_lattice_effect_output_gain") == 0) {
        gLGPTransientLatticeEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgptransient_lattice_effect_centre_bias") == 0) {
        gLGPTransientLatticeEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPTransientLatticeEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgptransient_lattice_effect_speed_scale") == 0) return gLGPTransientLatticeEffectSpeedScale;
    if (strcmp(name, "lgptransient_lattice_effect_output_gain") == 0) return gLGPTransientLatticeEffectOutputGain;
    if (strcmp(name, "lgptransient_lattice_effect_centre_bias") == 0) return gLGPTransientLatticeEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPTransientLatticeEffect

void LGPTransientLatticeEffect::cleanup() {}

const plugins::EffectMetadata& LGPTransientLatticeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Transient Lattice",
        "Snare and flux impacts drive a decaying interference scaffold",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// LGP Wavelet Mirror
// Principle: mirrored waveform crest field with beat-reinforced travelling ridge.
// ---------------------------------------------------------------------------

LGPWaveletMirrorEffect::LGPWaveletMirrorEffect() = default;

bool LGPWaveletMirrorEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_waveEnv = 0.0f;
    m_beatTrail = 0.0f;
    m_lastBeatMs = 0;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    return true;
}

void LGPWaveletMirrorEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        return;
    }
    const float master = (ctx.brightness / 255.0f) * m_audioPresence;

    float waveAvg = 0.0f;
    if (ctx.audio.available) {
        for (uint8_t s = 0; s < 8; ++s) {
            const uint8_t idx = static_cast<uint8_t>((s * 16u + ((ctx.rawTotalTimeMs >> 3) & 0x0Fu)) & 0x7Fu);
            waveAvg += fabsf(ctx.audio.getWaveformNormalized(idx));
        }
        waveAvg *= 0.125f;
    }

    const float waveTarget = ctx.audio.available
        ? fmaxf(clamp01f(ctx.audio.rms()), clamp01f(waveAvg))
        : fallbackSine(ctx.rawTotalTimeMs, 0.0012f, 0.6f);
    m_waveEnv = smoothTo(m_waveEnv, waveTarget, dtSignal, 0.09f);

    const bool beatTick = AudioReactivePolicy::audioBeatTick(ctx, 128.0f, m_lastBeatMs);
    if (beatTick) {
        m_beatTrail = 1.0f;
    } else {
        m_beatTrail = decay(m_beatTrail, dtSignal, 0.22f);
    }

    m_phase += 0.82f * (0.55f + 1.35f * m_waveEnv) * dtVisual;
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, EX_TAU);

    const float ridgePos = clamp01f(1.0f - m_beatTrail);
    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 30));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);
        const uint8_t idx = static_cast<uint8_t>((dist * 128u) / HALF_LENGTH);
        const uint8_t idxMirror = static_cast<uint8_t>(127u - idx);

        const float s1 = ctx.audio.available
            ? ctx.audio.getWaveformNormalized(idx)
            : sinf(m_phase * 2.8f + d * EX_TAU);
        const float s2 = ctx.audio.available
            ? ctx.audio.getWaveformNormalized(idxMirror)
            : sinf(m_phase * 2.8f + (1.0f - d) * EX_TAU);

        const float crest1 = powf(clamp01f(fabsf(s1)), 0.72f);
        const float crest2 = powf(clamp01f(fabsf(s2)), 0.72f);
        const float carrier1 = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.19f - m_phase * 4.5f);
        const float carrier2 = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 0.19f + m_phase * 4.5f + EX_PI * 0.5f);
        const float ridge = expf(-fabsf(d - ridgePos) * 11.0f) * m_beatTrail;

        const float i1 = clamp01f((0.22f + 0.78f * crest1) * (0.25f + 0.75f * carrier1) * (0.35f + 0.65f * m_waveEnv) + ridge * 0.72f);
        const float i2 = clamp01f((0.22f + 0.78f * crest2) * (0.25f + 0.75f * carrier2) * (0.35f + 0.65f * m_waveEnv) + ridge * 0.72f);

        const uint8_t br = toBrightness(0.5f * (i1 + i2), master);
        const uint8_t idxA = static_cast<uint8_t>(baseHue +
            static_cast<uint8_t>((0.5f * (crest1 + crest2)) * 34.0f) +
            static_cast<uint8_t>(d * 16.0f));
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}

void LGPWaveletMirrorEffect::cleanup() {}

const plugins::EffectMetadata& LGPWaveletMirrorEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Wavelet Mirror",
        "Waveform crest mirroring with beat-travel ridge reinforcement",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
