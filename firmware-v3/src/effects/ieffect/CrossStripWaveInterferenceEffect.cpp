#include "CrossStripWaveInterferenceEffect.h"

#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

inline float clamp01(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    return v;
}

inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

inline float fract01(float v) {
    v -= floorf(v);
    return (v < 0.0f) ? (v + 1.0f) : v;
}

inline float triangle01(float x) {
    const float f = fract01(x);
    return (f < 0.5f) ? (f * 2.0f) : ((1.0f - f) * 2.0f);
}

inline uint8_t toByte(float v) {
    v = clamp01(v);
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

inline CRGB scaledColour(const CRGB& colour, uint8_t scale) {
    CRGB out = colour;
    out.nscale8(scale);
    return out;
}

}  // namespace

bool CrossStripWaveInterferenceEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phaseOffsetCycles = 0.375f;
    m_wavePhase = 0.0f;
    m_speedHz = 0.12f;
    m_contrast = 0.88f;
    m_wavelengthLeds = 12.0f;
    return true;
}

void CrossStripWaveInterferenceEffect::render(plugins::EffectContext& ctx) {
    if (ctx.stripCount < 2 || ctx.stripLeds[0] == nullptr || ctx.stripLeds[1] == nullptr) {
        return;
    }

    const uint16_t stripLen = ctx.stripLength;
    if (stripLen == 0) {
        return;
    }

    ctx.dualChannelMode = true;

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float speedScale = 0.65f + (static_cast<float>(ctx.speed) / 50.0f) * 0.70f;
    m_wavePhase = fract01(m_wavePhase + dt * m_speedHz * speedScale);

    const float safeWavelength = clampf(m_wavelengthLeds, 8.0f, 24.0f);
    const float contrast = clamp01(m_contrast);
    const float base = 0.14f * (1.0f - contrast);
    const float gain = 0.55f + 0.45f * (static_cast<float>(ctx.intensity) / 255.0f);
    const float brightnessScale = static_cast<float>(ctx.brightness) / 255.0f;

    // Bounded dual-colour identity. Deliberately not hue-cycling.
    const CRGB stripAColour(0, 176, 255);    // cyan
    const CRGB stripBColour(255, 132, 18);   // amber

    const uint16_t centre = (ctx.stripCenter < stripLen) ? ctx.stripCenter : static_cast<uint16_t>((stripLen - 1) / 2);
    const uint16_t rightCentre = (centre + 1 < stripLen) ? static_cast<uint16_t>(centre + 1) : centre;

    for (uint16_t dist = 0; dist <= centre; ++dist) {
        const float distanceCycles = static_cast<float>(dist) / safeWavelength;
        const float phaseA = distanceCycles - m_wavePhase;
        const float phaseB = phaseA + m_phaseOffsetCycles;

        const float toothA = triangle01(phaseA);
        const float toothB = triangle01(phaseB);

        const float aLevel = clamp01((base + toothA * contrast) * gain * brightnessScale);
        const float bLevel = clamp01((base + toothB * contrast) * gain * brightnessScale);

        const CRGB colourA = scaledColour(stripAColour, toByte(aLevel));
        const CRGB colourB = scaledColour(stripBColour, toByte(bLevel));

        const uint16_t left = static_cast<uint16_t>(centre - dist);
        const uint16_t right = static_cast<uint16_t>(rightCentre + dist);

        ctx.stripLeds[0][left] = colourA;
        ctx.stripLeds[1][left] = colourB;
        if (right < stripLen) {
            ctx.stripLeds[0][right] = colourA;
            ctx.stripLeds[1][right] = colourB;
        }
    }
}

const plugins::EffectMetadata& CrossStripWaveInterferenceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Cross-Strip Wave Interference",
        "Phase-offset dual-strip standing waves",
        plugins::EffectCategory::QUANTUM,
        1,
        "LightwaveOS",
        plugins::EffectRoleFlags::DUAL_CHANNEL
    };
    return meta;
}

uint8_t CrossStripWaveInterferenceEffect::getParameterCount() const {
    return 4;
}

const plugins::EffectParameter* CrossStripWaveInterferenceEffect::getParameter(uint8_t index) const {
    static plugins::EffectParameter params[] = {
        plugins::EffectParameter("phaseOffset", "Phase Offset", 0.0f, 4.0f, 3.0f, plugins::EffectParameterType::INT, 1.0f, "wave", "", false),
        plugins::EffectParameter("wavelength", "Wavelength", 8.0f, 24.0f, 12.0f, plugins::EffectParameterType::FLOAT, 0.5f, "wave", "led", false),
        plugins::EffectParameter("contrast", "Contrast", 0.35f, 1.0f, 0.88f, plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false),
        plugins::EffectParameter("driftHz", "Drift", 0.0f, 0.35f, 0.12f, plugins::EffectParameterType::FLOAT, 0.01f, "wave", "Hz", false),
    };
    if (index >= (sizeof(params) / sizeof(params[0]))) {
        return nullptr;
    }
    return &params[index];
}

bool CrossStripWaveInterferenceEffect::setParameter(const char* name, float value) {
    if (name == nullptr) return false;

    if (strcmp(name, "phaseOffset") == 0) {
        const int slot = static_cast<int>(value + 0.5f);
        switch (slot) {
            case 0: m_phaseOffsetCycles = 0.0f; break;
            case 1: m_phaseOffsetCycles = 0.125f; break;  // pi/4
            case 2: m_phaseOffsetCycles = 0.25f; break;   // pi/2
            case 4: m_phaseOffsetCycles = 0.5f; break;    // pi
            case 3:
            default: m_phaseOffsetCycles = 0.375f; break; // 3pi/4
        }
        return true;
    }
    if (strcmp(name, "wavelength") == 0) {
        m_wavelengthLeds = clampf(value, 8.0f, 24.0f);
        return true;
    }
    if (strcmp(name, "contrast") == 0) {
        m_contrast = clampf(value, 0.35f, 1.0f);
        return true;
    }
    if (strcmp(name, "driftHz") == 0) {
        m_speedHz = clampf(value, 0.0f, 0.35f);
        return true;
    }
    return false;
}

float CrossStripWaveInterferenceEffect::getParameter(const char* name) const {
    if (name == nullptr) return 0.0f;
    if (strcmp(name, "phaseOffset") == 0) {
        if (m_phaseOffsetCycles < 0.0625f) return 0.0f;
        if (m_phaseOffsetCycles < 0.1875f) return 1.0f;
        if (m_phaseOffsetCycles < 0.3125f) return 2.0f;
        if (m_phaseOffsetCycles < 0.4375f) return 3.0f;
        return 4.0f;
    }
    if (strcmp(name, "wavelength") == 0) return m_wavelengthLeds;
    if (strcmp(name, "contrast") == 0) return m_contrast;
    if (strcmp(name, "driftHz") == 0) return m_speedHz;
    return 0.0f;
}

}  // namespace ieffect
}  // namespace effects
}  // namespace lightwaveos
