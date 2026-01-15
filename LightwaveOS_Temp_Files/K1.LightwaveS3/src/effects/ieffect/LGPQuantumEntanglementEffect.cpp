/**
 * @file LGPQuantumEntanglementEffect.cpp
 * @brief LGP Quantum Entanglement effect implementation
 */

#include "LGPQuantumEntanglementEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPQuantumEntanglementEffect::LGPQuantumEntanglementEffect()
    : m_collapseRadius(0.0f)
    , m_collapsing(false)
    , m_collapsed(false)
    , m_holdTime(0.0f)
    , m_collapsedHue(0)
    , m_quantumPhase(0.0f)
    , m_measurementTimer(0.0f)
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
    return true;
}

void LGPQuantumEntanglementEffect::render(plugins::EffectContext& ctx) {
    // EPR paradox visualisation with superposition and measurement
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    const int quantumN = 4;

    m_quantumPhase += speed * 0.1f;

    if (!m_collapsing && !m_collapsed) {
        m_measurementTimer += speed * 0.01f;

        if (m_measurementTimer > 1.0f + random8() / 255.0f) {
            m_collapsing = true;
            m_collapseRadius = 0.0f;
            m_collapsedHue = (uint8_t)(ctx.gHue + random8());
            m_measurementTimer = 0.0f;
        }
    } else if (m_collapsing) {
        m_collapseRadius += speed * 0.02f;

        if (m_collapseRadius >= 1.0f) {
            m_collapsing = false;
            m_collapsed = true;
            m_holdTime = 0.0f;
        }
    } else if (m_collapsed) {
        m_holdTime += speed * 0.02f;

        if (m_holdTime > 1.5f) {
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

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
