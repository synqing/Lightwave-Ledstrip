/**
 * @file LGPRileyDissonanceEffect.cpp
 * @brief LGP Riley Dissonance effect implementation
 */

#include "LGPRileyDissonanceEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.02f;
constexpr float kBaseFreq = 12.0f;
constexpr float kFreqMismatch = 0.08f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.005f, 0.08f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.001f, "timing", "x", false},
    {"base_freq", "Base Frequency", 4.0f, 24.0f, kBaseFreq, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"freq_mismatch", "Freq Mismatch", 0.01f, 0.30f, kFreqMismatch, plugins::EffectParameterType::FLOAT, 0.005f, "wave", "", false},
};
}

LGPRileyDissonanceEffect::LGPRileyDissonanceEffect()
    : m_patternPhase(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_baseFreq(kBaseFreq)
    , m_freqMismatch(kFreqMismatch)
{
}

bool LGPRileyDissonanceEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_patternPhase = 0.0f;
    m_phaseRate = kPhaseRate;
    m_baseFreq = kBaseFreq;
    m_freqMismatch = kFreqMismatch;
    return true;
}

void LGPRileyDissonanceEffect::render(plugins::EffectContext& ctx) {
    // Op art perceptual instability inspired by Bridget Riley
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    m_patternPhase += speed * m_phaseRate;

    float freq1 = m_baseFreq * (1.0f + m_freqMismatch / 2.0f);
    float freq2 = m_baseFreq * (1.0f - m_freqMismatch / 2.0f);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float pattern1 = sinf(normalizedDist * freq1 * TWO_PI + m_patternPhase);
        float pattern2 = sinf(normalizedDist * freq2 * TWO_PI - m_patternPhase);

        // Apply contrast enhancement
        float contrast = 1.0f + intensity * 4.0f;
        pattern1 = tanhf(pattern1 * contrast) / tanhf(contrast);
        pattern2 = tanhf(pattern2 * contrast) / tanhf(contrast);

        float rivalryZone = fabsf(pattern1 - pattern2);

        float bright1 = 128.0f + (pattern1 * 127.0f * intensity);
        float bright2 = 128.0f + (pattern2 * 127.0f * intensity);

        uint8_t brightness1 = (uint8_t)constrain(bright1, 0.0f, 255.0f);
        uint8_t brightness2 = (uint8_t)constrain(bright2, 0.0f, 255.0f);

        // Complementary colours increase dissonance
        uint8_t hue1 = ctx.gHue;
        uint8_t hue2 = (uint8_t)(ctx.gHue + 128);
        const uint8_t sat = 200;

        if (rivalryZone > 0.5f) {
            hue1 = (uint8_t)(hue1 + (uint8_t)(rivalryZone * 30.0f));
            hue2 = (uint8_t)(hue2 - (uint8_t)(rivalryZone * 30.0f));
        }

        uint8_t brightU8_1 = (uint8_t)((brightness1 * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue1, brightU8_1);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t brightU8_2 = (uint8_t)((brightness2 * ctx.brightness) / 255);
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue2, brightU8_2);
        }
    }
}

void LGPRileyDissonanceEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPRileyDissonanceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Riley Dissonance",
        "Op-art visual vibration",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPRileyDissonanceEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPRileyDissonanceEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPRileyDissonanceEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.005f, 0.08f);
        return true;
    }
    if (strcmp(name, "base_freq") == 0) {
        m_baseFreq = constrain(value, 4.0f, 24.0f);
        return true;
    }
    if (strcmp(name, "freq_mismatch") == 0) {
        m_freqMismatch = constrain(value, 0.01f, 0.30f);
        return true;
    }
    return false;
}

float LGPRileyDissonanceEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "base_freq") == 0) return m_baseFreq;
    if (strcmp(name, "freq_mismatch") == 0) return m_freqMismatch;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
