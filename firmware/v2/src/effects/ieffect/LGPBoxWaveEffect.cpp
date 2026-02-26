/**
 * @file LGPBoxWaveEffect.cpp
 * @brief LGP Box Wave effect implementation
 */

#include "LGPBoxWaveEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kBoxesPerSide = 6.0f;
constexpr float kPhaseRate = 0.05f;
constexpr float kPatternSharpness = 2.0f;

const plugins::EffectParameter kParameters[] = {
    {"boxes_per_side", "Boxes/Side", 3.0f, 12.0f, kBoxesPerSide, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"phase_rate", "Phase Rate", 0.01f, 0.20f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.005f, "timing", "x", false},
    {"pattern_sharpness", "Sharpness", 0.5f, 4.0f, kPatternSharpness, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
};
}

LGPBoxWaveEffect::LGPBoxWaveEffect()
    : m_boxMotionPhase(0.0f)
    , m_boxesPerSide(kBoxesPerSide)
    , m_phaseRate(kPhaseRate)
    , m_patternSharpness(kPatternSharpness)
{
}

bool LGPBoxWaveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_boxMotionPhase = 0.0f;
    m_boxesPerSide = kBoxesPerSide;
    m_phaseRate = kPhaseRate;
    m_patternSharpness = kPatternSharpness;
    return true;
}

void LGPBoxWaveEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN BOX WAVE - Creates controllable standing wave boxes from center
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // Box count: 3-12 boxes
    const float spatialFreq = m_boxesPerSide * PI / (float)HALF_LENGTH;

    m_boxMotionPhase += speedNorm * m_phaseRate;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Base box pattern from center
        float boxPhase = distFromCenter * spatialFreq;
        float boxPattern = sinf(boxPhase + m_boxMotionPhase);

        // Sharpness control
        boxPattern = tanhf(boxPattern * m_patternSharpness);

        // Convert to brightness
        uint8_t brightness = (uint8_t)(128.0f + 127.0f * boxPattern * intensityNorm);

        // Color wave overlay
        uint8_t colorIndex = (uint8_t)(ctx.gHue + (uint8_t)(distFromCenter * 2.0f));

        // Apply to both strips
        ctx.leds[i] = ctx.palette.getColor(colorIndex, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(colorIndex + 128), brightness);
        }
    }
}

void LGPBoxWaveEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPBoxWaveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Box Wave",
        "Square wave standing patterns",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPBoxWaveEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPBoxWaveEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPBoxWaveEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "boxes_per_side") == 0) {
        m_boxesPerSide = constrain(value, 3.0f, 12.0f);
        return true;
    }
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.01f, 0.20f);
        return true;
    }
    if (strcmp(name, "pattern_sharpness") == 0) {
        m_patternSharpness = constrain(value, 0.5f, 4.0f);
        return true;
    }
    return false;
}

float LGPBoxWaveEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "boxes_per_side") == 0) return m_boxesPerSide;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "pattern_sharpness") == 0) return m_patternSharpness;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
