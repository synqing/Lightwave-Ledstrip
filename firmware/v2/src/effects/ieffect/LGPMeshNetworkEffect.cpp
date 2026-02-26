/**
 * @file LGPMeshNetworkEffect.cpp
 * @brief LGP Mesh Network effect implementation
 */

#include "LGPMeshNetworkEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.02f;
constexpr int kNodeCount = 12;
constexpr float kConnectionFreq = 0.5f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.005f, 0.08f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.001f, "timing", "x", false},
    {"node_count", "Node Count", 4.0f, 24.0f, (float)kNodeCount, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"connection_freq", "Connection Freq", 0.2f, 1.2f, kConnectionFreq, plugins::EffectParameterType::FLOAT, 0.02f, "wave", "", false},
};
}

LGPMeshNetworkEffect::LGPMeshNetworkEffect()
    : m_phase(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_nodeCount(kNodeCount)
    , m_connectionFreq(kConnectionFreq)
{
}

bool LGPMeshNetworkEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_phaseRate = kPhaseRate;
    m_nodeCount = kNodeCount;
    m_connectionFreq = kConnectionFreq;
    return true;
}

void LGPMeshNetworkEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Interconnected node patterns like neural networks
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * m_phaseRate * 60.0f * dt;
    const int nodeCount = m_nodeCount;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int n = 0; n < nodeCount; n++) {
        float nodePos = (float)n / (float)nodeCount * (float)HALF_LENGTH;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distToNode = fabsf((float)centerPairDistance((uint16_t)i) - nodePos);

            if (distToNode < 3.0f) {
                // Node core
                uint8_t nodeBright = (uint8_t)(255.0f * intensityNorm);
                ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + (n * 20)), nodeBright);
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + (n * 20) + 128),
                                                                      nodeBright);
                }
            } else if (distToNode < 20.0f) {
                // Connections to nearby nodes (sin(k*dist + phase) = INWARD propagation - intentional design)
                float connection = sinf(distToNode * m_connectionFreq + m_phase + n);
                connection *= expf(-distToNode * 0.1f);

                uint8_t connBright = (uint8_t)(fabsf(connection) * 128.0f * intensityNorm);
                CRGB connColor = ctx.palette.getColor((uint8_t)(ctx.gHue + (n * 20)), connBright);
                ctx.leds[i] = connColor;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    CRGB connColor2 = ctx.palette.getColor((uint8_t)(ctx.gHue + (n * 20) + 128), connBright);
                    ctx.leds[i + STRIP_LENGTH] = connColor2;
                }
            }
        }
    }
}

void LGPMeshNetworkEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPMeshNetworkEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Mesh Network",
        "Interconnected node graph",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

uint8_t LGPMeshNetworkEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPMeshNetworkEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPMeshNetworkEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.005f, 0.08f);
        return true;
    }
    if (strcmp(name, "node_count") == 0) {
        m_nodeCount = (int)constrain(value, 4.0f, 24.0f);
        return true;
    }
    if (strcmp(name, "connection_freq") == 0) {
        m_connectionFreq = constrain(value, 0.2f, 1.2f);
        return true;
    }
    return false;
}

float LGPMeshNetworkEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "node_count") == 0) return (float)m_nodeCount;
    if (strcmp(name, "connection_freq") == 0) return m_connectionFreq;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
