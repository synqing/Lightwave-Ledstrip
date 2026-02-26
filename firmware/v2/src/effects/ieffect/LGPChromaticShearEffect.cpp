/**
 * @file LGPChromaticShearEffect.cpp
 * @brief LGP Chromatic Shear effect implementation
 */

#include "LGPChromaticShearEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseStep = 1.0f;
constexpr int kPaletteStep = 2;
constexpr float kShearAmount = 128.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase_step", "Phase Step", 0.25f, 4.0f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"palette_step", "Palette Step", 1.0f, 8.0f, (float)kPaletteStep, plugins::EffectParameterType::INT, 1.0f, "colour", "", false},
    {"shear_amount", "Shear Amount", 32.0f, 255.0f, kShearAmount, plugins::EffectParameterType::FLOAT, 1.0f, "wave", "", false},
};
}

LGPChromaticShearEffect::LGPChromaticShearEffect()
    : m_phase(0)
    , m_paletteOffset(0)
    , m_lastUpdateFrame(0)
    , m_phaseStep(kPhaseStep)
    , m_paletteStep(kPaletteStep)
    , m_shearAmount(kShearAmount)
{
}

bool LGPChromaticShearEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0;
    m_paletteOffset = 0;
    m_lastUpdateFrame = 0;
    m_phaseStep = kPhaseStep;
    m_paletteStep = kPaletteStep;
    m_shearAmount = kShearAmount;
    return true;
}

void LGPChromaticShearEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Color planes sliding with velocity shear
    m_phase = (uint16_t)(m_phase + (uint16_t)(ctx.speed * m_phaseStep));

    // Palette rotation
    if (ctx.frameNumber - m_lastUpdateFrame > 5) {
        m_paletteOffset = (uint8_t)(m_paletteOffset + m_paletteStep);
        m_lastUpdateFrame = ctx.frameNumber;
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i);
        uint8_t distPos = (uint8_t)((distFromCenter * 255.0f) / (float)HALF_LENGTH);

        // Apply shear transformation
        uint16_t shearOffset = ((uint16_t)distPos * (uint16_t)m_shearAmount) >> 3;

        // Left strip: base hue + shear
        uint8_t leftHue = (uint8_t)(m_paletteOffset + distPos + (m_phase >> 8) + (shearOffset >> 8));
        uint8_t leftBright = ctx.brightness;

        // Right strip: complementary hue + inverse shear
        uint8_t rightHue = (uint8_t)(m_paletteOffset + distPos + 120 - (m_phase >> 8) - (shearOffset >> 8));
        uint8_t rightBright = ctx.brightness;

        // Add interference at center
        if (centerPairDistance(i) < 20) {
            uint8_t centerBlend = (uint8_t)(255 - centerPairDistance(i) * 12);
            leftBright = scale8(leftBright, (uint8_t)(255 - (centerBlend >> 1)));
            rightBright = scale8(rightBright, (uint8_t)(255 - (centerBlend >> 1)));
        }

        ctx.leds[i] = ctx.palette.getColor(leftHue, leftBright);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(rightHue, rightBright);
        }
    }
}

void LGPChromaticShearEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPChromaticShearEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chromatic Shear",
        "Color-splitting shear effect",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPChromaticShearEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPChromaticShearEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPChromaticShearEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.25f, 4.0f);
        return true;
    }
    if (strcmp(name, "palette_step") == 0) {
        m_paletteStep = (int)constrain(value, 1.0f, 8.0f);
        return true;
    }
    if (strcmp(name, "shear_amount") == 0) {
        m_shearAmount = constrain(value, 32.0f, 255.0f);
        return true;
    }
    return false;
}

float LGPChromaticShearEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    if (strcmp(name, "palette_step") == 0) return (float)m_paletteStep;
    if (strcmp(name, "shear_amount") == 0) return m_shearAmount;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
