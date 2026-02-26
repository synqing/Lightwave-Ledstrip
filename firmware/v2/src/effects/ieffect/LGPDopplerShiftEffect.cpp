/**
 * @file LGPDopplerShiftEffect.cpp
 * @brief LGP Doppler Shift effect implementation
 */

#include "LGPDopplerShiftEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kMotionScale = 5.0f;
constexpr float kVelocityScale = 10.0f;
constexpr float kShiftStrength = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"motion_scale", "Motion Scale", 1.0f, 12.0f, kMotionScale, plugins::EffectParameterType::FLOAT, 0.5f, "timing", "x", false},
    {"velocity_scale", "Velocity Scale", 4.0f, 20.0f, kVelocityScale, plugins::EffectParameterType::FLOAT, 0.5f, "wave", "x", false},
    {"shift_strength", "Shift Strength", 0.3f, 2.0f, kShiftStrength, plugins::EffectParameterType::FLOAT, 0.05f, "colour", "x", false},
};
}

LGPDopplerShiftEffect::LGPDopplerShiftEffect()
    : m_sourcePosition(0.0f)
    , m_motionScale(kMotionScale)
    , m_velocityScale(kVelocityScale)
    , m_shiftStrength(kShiftStrength)
{
}

bool LGPDopplerShiftEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_sourcePosition = 0.0f;
    m_motionScale = kMotionScale;
    m_velocityScale = kVelocityScale;
    m_shiftStrength = kShiftStrength;
    return true;
}

void LGPDopplerShiftEffect::render(plugins::EffectContext& ctx) {
    // Moving colors shift frequency based on velocity
    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    m_sourcePosition += speed * m_motionScale * 60.0f * dt;  // dt-corrected

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        float relativePos = i - fmodf(m_sourcePosition, (float)STRIP_LENGTH);
        float velocity = speed * m_velocityScale;

        float dopplerFactor;
        if (relativePos > 0.0f) {
            dopplerFactor = 1.0f - (velocity / 100.0f);
        } else {
            dopplerFactor = 1.0f + (velocity / 100.0f);
        }

        uint8_t shiftedHue = ctx.gHue;
        if (dopplerFactor > 1.0f) {
            shiftedHue = (uint8_t)(ctx.gHue - (uint8_t)(30.0f * m_shiftStrength * (dopplerFactor - 1.0f)));
        } else {
            shiftedHue = (uint8_t)(ctx.gHue + (uint8_t)(30.0f * m_shiftStrength * (1.0f - dopplerFactor)));
        }

        uint8_t brightness = (uint8_t)(255.0f * intensity * (1.0f - distFromCenter / HALF_LENGTH));

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(shiftedHue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(shiftedHue + 90), brightU8);
        }
    }
}

void LGPDopplerShiftEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPDopplerShiftEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Doppler Shift",
        "Red/Blue shift based on velocity",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPDopplerShiftEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPDopplerShiftEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPDopplerShiftEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "motion_scale") == 0) {
        m_motionScale = constrain(value, 1.0f, 12.0f);
        return true;
    }
    if (strcmp(name, "velocity_scale") == 0) {
        m_velocityScale = constrain(value, 4.0f, 20.0f);
        return true;
    }
    if (strcmp(name, "shift_strength") == 0) {
        m_shiftStrength = constrain(value, 0.3f, 2.0f);
        return true;
    }
    return false;
}

float LGPDopplerShiftEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "motion_scale") == 0) return m_motionScale;
    if (strcmp(name, "velocity_scale") == 0) return m_velocityScale;
    if (strcmp(name, "shift_strength") == 0) return m_shiftStrength;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
