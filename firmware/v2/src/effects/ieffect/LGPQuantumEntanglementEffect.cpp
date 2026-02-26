/**
 * @file LGPQuantumEntanglementEffect.cpp
 * @brief LGP Quantum Entanglement effect implementation
 */

#include "LGPQuantumEntanglementEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kMeasurementRate = 0.01f;
constexpr float kCollapseRate = 0.02f;
constexpr float kHoldDuration = 1.5f;

const plugins::EffectParameter kParameters[] = {
    {"measurement_rate", "Measurement Rate", 0.002f, 0.04f, kMeasurementRate, plugins::EffectParameterType::FLOAT, 0.001f, "timing", "x", false},
    {"collapse_rate", "Collapse Rate", 0.005f, 0.08f, kCollapseRate, plugins::EffectParameterType::FLOAT, 0.001f, "timing", "x", false},
    {"hold_duration", "Hold Duration", 0.3f, 4.0f, kHoldDuration, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "s", false},
};
}

LGPQuantumEntanglementEffect::LGPQuantumEntanglementEffect()
    : m_collapseRadius(0.0f)
    , m_collapsing(false)
    , m_collapsed(false)
    , m_holdTime(0.0f)
    , m_collapsedHue(0)
    , m_quantumPhase(0.0f)
    , m_measurementTimer(0.0f)
    , m_measurementRate(kMeasurementRate)
    , m_collapseRate(kCollapseRate)
    , m_holdDuration(kHoldDuration)
{
}

bool LGPQuantumEntanglementEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_collapseRadius = 0.0f;
    m_collapsing = false;
    m_collapsed = false;
    m_holdTime = 0.0f;
    m_collapsedHue = 0;
    m_quantumPhase = 0.0f;
    m_measurementTimer = 0.0f;
    m_measurementRate = kMeasurementRate;
    m_collapseRate = kCollapseRate;
    m_holdDuration = kHoldDuration;
    return true;
}

void LGPQuantumEntanglementEffect::render(plugins::EffectContext& ctx) {
    // EPR paradox visualisation with superposition and measurement
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    const int quantumN = 4;

    m_quantumPhase += speed * 0.1f;

    if (!m_collapsing && !m_collapsed) {
        m_measurementTimer += speed * m_measurementRate;

        if (m_measurementTimer > 1.0f + random8() / 255.0f) {
            m_collapsing = true;
            m_collapseRadius = 0.0f;
            m_collapsedHue = (uint8_t)(ctx.gHue + random8());
            m_measurementTimer = 0.0f;
        }
    } else if (m_collapsing) {
        m_collapseRadius += speed * m_collapseRate;

        if (m_collapseRadius >= 1.0f) {
            m_collapsing = false;
            m_collapsed = true;
            m_holdTime = 0.0f;
        }
    } else if (m_collapsed) {
        m_holdTime += speed * 0.02f;

        if (m_holdTime > m_holdDuration) {
            m_collapsed = false;
            m_collapseRadius = 0.0f;
        }
    }

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        uint8_t hue1 = 0;
        uint8_t hue2 = 0;
        uint8_t brightness1 = 0;
        uint8_t brightness2 = 0;

        if (!m_collapsing && !m_collapsed) {
            float waveFunc = sinf(quantumN * PI * normalizedDist);
            float probability = waveFunc * waveFunc;

            float fluctuation = sinf(m_quantumPhase * 3.0f + i * 0.2f) *
                                cosf(m_quantumPhase * 5.0f - i * 0.15f) * intensity;

            hue1 = (uint8_t)(ctx.gHue + (uint8_t)(sinf(m_quantumPhase + i * 0.1f) * 15.0f));
            hue2 = (uint8_t)(ctx.gHue + (uint8_t)(cosf(m_quantumPhase * 1.3f - i * 0.12f) * 15.0f));

            brightness1 = (uint8_t)(80.0f + probability * 100.0f + fabsf(fluctuation) * 75.0f);
            brightness2 = (uint8_t)(80.0f + probability * 100.0f + fabsf(fluctuation) * 75.0f);

        } else if (m_collapsing) {
            if (normalizedDist < m_collapseRadius) {
                hue1 = m_collapsedHue;
                hue2 = (uint8_t)(m_collapsedHue + 128);

                float collapseEdge = m_collapseRadius - normalizedDist;
                float edgeFactor = constrain(collapseEdge * 10.0f, 0.0f, 1.0f);

                brightness1 = (uint8_t)(180.0f * edgeFactor + 50.0f);
                brightness2 = (uint8_t)(180.0f * edgeFactor + 50.0f);

            } else {
                float chaos = sinf(m_quantumPhase * 5.0f + i * 0.3f) * intensity;
                hue1 = (uint8_t)(ctx.gHue + (uint8_t)(chaos * 40.0f));
                hue2 = (uint8_t)(ctx.gHue + (uint8_t)(chaos * 40.0f) + random8(30));
                brightness1 = (uint8_t)(60.0f + fabsf(chaos) * 50.0f);
                brightness2 = (uint8_t)(60.0f + fabsf(chaos) * 50.0f);
            }

        } else {
            hue1 = m_collapsedHue;
            hue2 = (uint8_t)(m_collapsedHue + 128);

            float pulse = sinf(m_quantumPhase) * 0.1f + 0.9f;
            brightness1 = (uint8_t)(200.0f * pulse);
            brightness2 = (uint8_t)(200.0f * pulse);
        }

        ctx.leds[i] = ctx.palette.getColor(hue1, brightness1);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue2, brightness2);
        }
    }
}

void LGPQuantumEntanglementEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPQuantumEntanglementEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Quantum Entanglement",
        "Correlated state collapse",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPQuantumEntanglementEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPQuantumEntanglementEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPQuantumEntanglementEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "measurement_rate") == 0) {
        m_measurementRate = constrain(value, 0.002f, 0.04f);
        return true;
    }
    if (strcmp(name, "collapse_rate") == 0) {
        m_collapseRate = constrain(value, 0.005f, 0.08f);
        return true;
    }
    if (strcmp(name, "hold_duration") == 0) {
        m_holdDuration = constrain(value, 0.3f, 4.0f);
        return true;
    }
    return false;
}

float LGPQuantumEntanglementEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "measurement_rate") == 0) return m_measurementRate;
    if (strcmp(name, "collapse_rate") == 0) return m_collapseRate;
    if (strcmp(name, "hold_duration") == 0) return m_holdDuration;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
