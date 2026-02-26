/**
 * @file BeatPulseLGPInterferenceEffect.cpp
 * @brief Beat Pulse with LGP optical interference exploitation
 *
 * Exploits the dual edge-lit LGP architecture by driving Strip 1 and Strip 2
 * with configurable phase relationships, creating standing wave interference
 * patterns that are impossible on single-strip displays.
 *
 * Effect ID: 120
 */

#include "BeatPulseLGPInterferenceEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:BeatPulseLGPInterferenceEffect
namespace {
constexpr float kBeatPulseLGPInterferenceEffectSpeedScale = 1.0f;
constexpr float kBeatPulseLGPInterferenceEffectOutputGain = 1.0f;
constexpr float kBeatPulseLGPInterferenceEffectCentreBias = 1.0f;

float gBeatPulseLGPInterferenceEffectSpeedScale = kBeatPulseLGPInterferenceEffectSpeedScale;
float gBeatPulseLGPInterferenceEffectOutputGain = kBeatPulseLGPInterferenceEffectOutputGain;
float gBeatPulseLGPInterferenceEffectCentreBias = kBeatPulseLGPInterferenceEffectCentreBias;

const lightwaveos::plugins::EffectParameter kBeatPulseLGPInterferenceEffectParameters[] = {
    {"beat_pulse_lgpinterference_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kBeatPulseLGPInterferenceEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"beat_pulse_lgpinterference_effect_output_gain", "Output Gain", 0.25f, 2.0f, kBeatPulseLGPInterferenceEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"beat_pulse_lgpinterference_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kBeatPulseLGPInterferenceEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:BeatPulseLGPInterferenceEffect

namespace lightwaveos::effects::ieffect {

// ============================================================================
// Constants
// ============================================================================

constexpr float LGP_PI = 3.14159265358979f;
constexpr float LGP_TWO_PI = 6.28318530717959f;

// Motion phase drift speed (radians per second) - creates slow standing wave animation
constexpr float PHASE_DRIFT_SPEED = 0.8f;
bool  BeatPulseLGPInterferenceEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:BeatPulseLGPInterferenceEffect
    gBeatPulseLGPInterferenceEffectSpeedScale = kBeatPulseLGPInterferenceEffectSpeedScale;
    gBeatPulseLGPInterferenceEffectOutputGain = kBeatPulseLGPInterferenceEffectOutputGain;
    gBeatPulseLGPInterferenceEffectCentreBias = kBeatPulseLGPInterferenceEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:BeatPulseLGPInterferenceEffect

    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    m_phaseMode = LGPPhaseMode::ANTI_PHASE;
    m_spatialFreq = 4.0f;
    m_motionPhase = 0.0f;
    return true;
}

void BeatPulseLGPInterferenceEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // LGP INTERFERENCE: Dual-strip phase control for optical standing waves
    // =========================================================================

    // --- Beat source ---
    const bool beatTick = BeatPulseTiming::computeBeatTick(ctx, m_fallbackBpm, m_lastBeatTimeMs);

    // --- Update beatIntensity using HTML parity maths ---
    const float dt = ctx.getSafeRawDeltaSeconds();
    BeatPulseHTML::updateBeatIntensity(m_beatIntensity, beatTick, dt);

    // --- Update motion phase for standing wave animation ---
    m_motionPhase += PHASE_DRIFT_SPEED * dt;
    if (m_motionPhase > LGP_TWO_PI) {
        m_motionPhase -= LGP_TWO_PI;
    }

    // --- Ring position (OUTWARD expansion) ---
    const float htmlCentre = BeatPulseHTML::ringCentre01(m_beatIntensity);
    const float ringPos = htmlCentre;  // Centre -> edge

    // --- Spatial frequency for interference pattern ---
    const float spatialK = m_spatialFreq * LGP_PI / static_cast<float>(HALF_LENGTH);

    // --- Phase offset for Strip 2 based on mode ---
    float strip2PhaseOffset = 0.0f;
    switch (m_phaseMode) {
        case LGPPhaseMode::IN_PHASE:
            strip2PhaseOffset = 0.0f;
            break;
        case LGPPhaseMode::QUADRATURE:
            strip2PhaseOffset = PI * 0.5f;
            break;
        case LGPPhaseMode::ANTI_PHASE:
            strip2PhaseOffset = PI;
            break;
    }

    // --- Render both strips with phase relationship ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // HTML parity triangle profile for the expanding ring
        const float diff = fabsf(dist01 - ringPos);
        const float waveHit = 1.0f - fminf(1.0f, diff * 3.0f);
        const float ringIntensity = fmaxf(0.0f, waveHit) * m_beatIntensity;

        // Spatial interference modulation (standing wave pattern)
        // This creates "boxes" of constructive/destructive interference
        const float spatialWave = sinf(static_cast<float>(dist) * spatialK + m_motionPhase);
        const float spatialMod = 0.7f + 0.3f * spatialWave;  // 0.4 to 1.0 range

        // --- Strip 1: Base intensity with spatial modulation ---
        const float strip1Intensity = ringIntensity * spatialMod;
        const float strip1Bright = BeatPulseHTML::brightnessFactor(strip1Intensity);
        const float strip1White = BeatPulseHTML::whiteMix(strip1Intensity);

        // --- Strip 2: Phase-shifted spatial modulation ---
        const float spatialWave2 = sinf(static_cast<float>(dist) * spatialK + m_motionPhase + strip2PhaseOffset);
        const float spatialMod2 = 0.7f + 0.3f * spatialWave2;

        float strip2Intensity = ringIntensity * spatialMod2;

        // For anti-phase, also invert the ring intensity for maximum interference
        if (m_phaseMode == LGPPhaseMode::ANTI_PHASE) {
            // Blend between normal and inverted based on spatial position
            // This creates alternating bright/dark nodes between strips
            const float invertBlend = 0.5f + 0.5f * spatialWave;
            strip2Intensity = ringIntensity * (1.0f - invertBlend * 0.6f) * spatialMod2;
        }

        const float strip2Bright = BeatPulseHTML::brightnessFactor(strip2Intensity);
        const float strip2White = BeatPulseHTML::whiteMix(strip2Intensity);

        // Palette colour by distance
        const uint8_t paletteIdx = floatToByte(dist01);

        // --- Set Strip 1 LEDs (centre-origin symmetric) ---
        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;

        CRGB c1 = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, strip1Bright));
        ColourUtil::addWhiteSaturating(c1, floatToByte(strip1White));

        if (left1 < ctx.ledCount) ctx.leds[left1] = c1;
        if (right1 < ctx.ledCount) ctx.leds[right1] = c1;

        // --- Set Strip 2 LEDs (offset by STRIP_LENGTH = 160) ---
        uint16_t left2 = 160 + CENTER_LEFT - dist;
        uint16_t right2 = 160 + CENTER_RIGHT + dist;

        CRGB c2 = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, strip2Bright));
        ColourUtil::addWhiteSaturating(c2, floatToByte(strip2White));

        if (left2 < ctx.ledCount) ctx.leds[left2] = c2;
        if (right2 < ctx.ledCount) ctx.leds[right2] = c2;
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:BeatPulseLGPInterferenceEffect
uint8_t BeatPulseLGPInterferenceEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kBeatPulseLGPInterferenceEffectParameters) / sizeof(kBeatPulseLGPInterferenceEffectParameters[0]));
}

const plugins::EffectParameter* BeatPulseLGPInterferenceEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kBeatPulseLGPInterferenceEffectParameters[index];
}

bool BeatPulseLGPInterferenceEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "beat_pulse_lgpinterference_effect_speed_scale") == 0) {
        gBeatPulseLGPInterferenceEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_lgpinterference_effect_output_gain") == 0) {
        gBeatPulseLGPInterferenceEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_lgpinterference_effect_centre_bias") == 0) {
        gBeatPulseLGPInterferenceEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float BeatPulseLGPInterferenceEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "beat_pulse_lgpinterference_effect_speed_scale") == 0) return gBeatPulseLGPInterferenceEffectSpeedScale;
    if (strcmp(name, "beat_pulse_lgpinterference_effect_output_gain") == 0) return gBeatPulseLGPInterferenceEffectOutputGain;
    if (strcmp(name, "beat_pulse_lgpinterference_effect_centre_bias") == 0) return gBeatPulseLGPInterferenceEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:BeatPulseLGPInterferenceEffect

void BeatPulseLGPInterferenceEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseLGPInterferenceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (LGP Interference)",
        "Dual-strip interference: standing waves exploit LGP optics",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}


} // namespace lightwaveos::effects::ieffect
