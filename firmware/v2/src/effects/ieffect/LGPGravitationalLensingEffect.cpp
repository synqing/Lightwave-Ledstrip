/**
 * @file LGPGravitationalLensingEffect.cpp
 * @brief LGP Gravitational Lensing effect implementation
 */

#include "LGPGravitationalLensingEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr int kMassCount = 2;
constexpr float kMassStrengthScale = 1.0f;
constexpr float kDeflectionGain = 20.0f;
constexpr float kPhaseStep = 0.25f;

const plugins::EffectParameter kParameters[] = {
    {"mass_count", "Mass Count", 1.0f, 3.0f, (float)kMassCount, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"mass_strength_scale", "Mass Strength", 0.2f, 2.0f, kMassStrengthScale, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"deflection_gain", "Deflection Gain", 8.0f, 36.0f, kDeflectionGain, plugins::EffectParameterType::FLOAT, 0.5f, "wave", "", false},
    {"phase_step", "Phase Step", 0.1f, 1.5f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", true},
};
}

LGPGravitationalLensingEffect::LGPGravitationalLensingEffect()
    : m_time(0)
    , m_massPos{40.0f, 80.0f, 120.0f}
    , m_massVel{0.5f, -0.3f, 0.4f}
    , m_massCount(kMassCount)
    , m_massStrengthScale(kMassStrengthScale)
    , m_deflectionGain(kDeflectionGain)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPGravitationalLensingEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_massPos[0] = 40.0f;
    m_massPos[1] = 80.0f;
    m_massPos[2] = 120.0f;
    m_massVel[0] = 0.5f;
    m_massVel[1] = -0.3f;
    m_massVel[2] = 0.4f;
    m_massCount = kMassCount;
    m_massStrengthScale = kMassStrengthScale;
    m_deflectionGain = kDeflectionGain;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPGravitationalLensingEffect::render(plugins::EffectContext& ctx) {
    // Light bends around invisible massive objects creating Einstein rings
    m_time = (uint16_t)(m_time + (uint16_t)(ctx.speed * m_phaseStep));

    float speedNorm = ctx.speed / 50.0f;
    const uint8_t massCount = (uint8_t)m_massCount;
    float massStrength = (ctx.brightness / 255.0f) * m_massStrengthScale;

    // Update mass positions
    for (uint8_t m = 0; m < massCount; m++) {
        m_massPos[m] += m_massVel[m] * speedNorm;

        if (m_massPos[m] < 20.0f || m_massPos[m] > (float)STRIP_LENGTH - 20.0f) {
            m_massVel[m] = -m_massVel[m];
        }
    }

    fill_solid(ctx.leds, ctx.ledCount, CRGB::Black);

    // Generate light rays from center
    for (int16_t ray = -40; ray <= 40; ray += 2) {
        for (int8_t direction = -1; direction <= 1; direction += 2) {
            float rayPos = (float)CENTER_LEFT;
            float rayAngle = ray * 0.02f * direction;

            for (uint8_t step = 0; step < 80; step++) {
                float totalDeflection = 0.0f;

                for (uint8_t m = 0; m < massCount; m++) {
                    float dist = fabsf(rayPos - m_massPos[m]);
                    if (dist < 40.0f && dist > 1.0f) {
                        float deflection = massStrength * m_deflectionGain / (dist * dist);
                        if (rayPos > m_massPos[m]) {
                            deflection = -deflection;
                        }
                        totalDeflection += deflection;
                    }
                }

                rayAngle += totalDeflection * 0.01f;
                rayPos += cosf(rayAngle) * 2.0f * direction;

                int16_t pixelPos = (int16_t)rayPos;
                if (pixelPos >= 0 && pixelPos < STRIP_LENGTH) {
                    uint8_t paletteIndex = (uint8_t)(fabsf(totalDeflection) * 20.0f);
                    uint8_t brightness = (uint8_t)(255 - step * 3);

                    // Clamp brightness to avoid white saturation
                    if (fabsf(totalDeflection) > 0.5f) {
                        brightness = 240;  // Slightly below 255 to avoid white guardrail
                        paletteIndex = (uint8_t)(fabsf(totalDeflection) * 30.0f);  // Use deflection-based color
                    }

                    ctx.leds[pixelPos] += ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
                    if (pixelPos + STRIP_LENGTH < ctx.ledCount) {
                        ctx.leds[pixelPos + STRIP_LENGTH] += ctx.palette.getColor(
                            (uint8_t)(ctx.gHue + paletteIndex + 64),
                            brightness);
                    }
                }

                if (rayPos < 0.0f || rayPos >= STRIP_LENGTH) {
                    break;
                }
            }
        }
    }
}

void LGPGravitationalLensingEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGravitationalLensingEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Gravitational Lensing",
        "Light bending around mass",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPGravitationalLensingEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPGravitationalLensingEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPGravitationalLensingEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "mass_count") == 0) {
        m_massCount = (int)constrain(value, 1.0f, 3.0f);
        return true;
    }
    if (strcmp(name, "mass_strength_scale") == 0) {
        m_massStrengthScale = constrain(value, 0.2f, 2.0f);
        return true;
    }
    if (strcmp(name, "deflection_gain") == 0) {
        m_deflectionGain = constrain(value, 8.0f, 36.0f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.1f, 1.5f);
        return true;
    }
    return false;
}

float LGPGravitationalLensingEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "mass_count") == 0) return (float)m_massCount;
    if (strcmp(name, "mass_strength_scale") == 0) return m_massStrengthScale;
    if (strcmp(name, "deflection_gain") == 0) return m_deflectionGain;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
