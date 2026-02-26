/**
 * @file LGPConcentricRingsEffect.cpp
 * @brief LGP Concentric Rings effect implementation
 */

#include "LGPConcentricRingsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.1f;
constexpr float kRingCount = 10.0f;
constexpr float kRingSharpness = 2.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.01f, 0.30f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.005f, "timing", "x", false},
    {"ring_count", "Ring Count", 3.0f, 18.0f, kRingCount, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"ring_sharpness", "Ring Sharpness", 0.5f, 4.0f, kRingSharpness, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
};
}

LGPConcentricRingsEffect::LGPConcentricRingsEffect()
    : m_phase(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_ringCount(kRingCount)
    , m_ringSharpness(kRingSharpness)
{
}

bool LGPConcentricRingsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_phaseRate = kPhaseRate;
    m_ringCount = kRingCount;
    m_ringSharpness = kRingSharpness;
    return true;
}

void LGPConcentricRingsEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Radial standing waves create ring patterns
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * m_phaseRate * 60.0f * dt;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Bessel function-like (sin(k*dist + phase) = INWARD motion - intentional design)
        float bessel = sinf(distFromCenter * m_ringCount * 0.2f + m_phase);
        bessel *= 1.0f / sqrtf(normalizedDist + 0.1f);

        // Sharp ring edges
        float rings = tanhf(bessel * m_ringSharpness);

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * rings * intensityNorm);

        ctx.leds[i] = ctx.palette.getColor(ctx.gHue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + 128), brightness);
        }
    }
}

void LGPConcentricRingsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPConcentricRingsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Concentric Rings",
        "Expanding circular rings",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

uint8_t LGPConcentricRingsEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPConcentricRingsEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPConcentricRingsEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.01f, 0.30f);
        return true;
    }
    if (strcmp(name, "ring_count") == 0) {
        m_ringCount = constrain(value, 3.0f, 18.0f);
        return true;
    }
    if (strcmp(name, "ring_sharpness") == 0) {
        m_ringSharpness = constrain(value, 0.5f, 4.0f);
        return true;
    }
    return false;
}

float LGPConcentricRingsEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "ring_count") == 0) return m_ringCount;
    if (strcmp(name, "ring_sharpness") == 0) return m_ringSharpness;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
