/**
 * @file LGPCausticShardsEffect.cpp
 * @brief LGP Caustic Shards effect implementation
 */

#include "LGPCausticShardsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstdint>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPCausticShardsEffect
namespace {
constexpr float kLGPCausticShardsEffectSpeedScale = 1.0f;
constexpr float kLGPCausticShardsEffectOutputGain = 1.0f;
constexpr float kLGPCausticShardsEffectCentreBias = 1.0f;

float gLGPCausticShardsEffectSpeedScale = kLGPCausticShardsEffectSpeedScale;
float gLGPCausticShardsEffectOutputGain = kLGPCausticShardsEffectOutputGain;
float gLGPCausticShardsEffectCentreBias = kLGPCausticShardsEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPCausticShardsEffectParameters[] = {
    {"lgpcaustic_shards_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPCausticShardsEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpcaustic_shards_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPCausticShardsEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpcaustic_shards_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPCausticShardsEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPCausticShardsEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

LGPCausticShardsEffect::LGPCausticShardsEffect()
    : m_phase1(0.0f)
    , m_phase2(0.0f)
    , m_phase3(0.0f)
{
}

bool LGPCausticShardsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPCausticShardsEffect
    gLGPCausticShardsEffectSpeedScale = kLGPCausticShardsEffectSpeedScale;
    gLGPCausticShardsEffectOutputGain = kLGPCausticShardsEffectOutputGain;
    gLGPCausticShardsEffectCentreBias = kLGPCausticShardsEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPCausticShardsEffect

    m_phase1 = 0.0f;
    m_phase2 = 0.0f;
    m_phase3 = 0.0f;
    return true;
}

void LGPCausticShardsEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN CAUSTIC SHARDS - Interference field with sharp glints
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_phase1 += 0.010f + 0.020f * speedNorm;
    m_phase2 += 0.012f + 0.030f * speedNorm;
    m_phase3 += 0.018f + 0.055f * speedNorm;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        float base =
            sinf(dist * 0.050f + m_phase1)
            + 0.7f * sinf(dist * 0.150f + m_phase2)
            + 0.5f * sinf(dist * 0.300f + m_phase3);
        base = tanhf(base / 2.2f);
        const float field = 0.5f + 0.5f * base;

        float shard = sinf(dist * 0.85f - m_phase1 * 3.0f);
        shard = clamp01(shard);
        shard = shard * shard * shard * shard;

        shard *= clamp01((field - 0.55f) * 2.2f);

        const uint32_t h = hash32((uint32_t)i ^ (uint32_t)(m_phase1 * 1000.0f));
        const float mask = ((h & 1023u) < 40u) ? 1.0f : 0.0f;
        shard *= mask;

        const float wave = clamp01(0.78f * field + 0.22f * shard);
        const float baseLit = 0.10f;
        const float out = clamp01(baseLit + (1.0f - baseLit) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        uint8_t hueA = (uint8_t)(ctx.gHue + (int)(dist * 0.6f) + (int)(field * 36.0f));
        if (shard > 0.2f) {
            hueA = (uint8_t)(hueA + 20u);
        }

        ctx.leds[i] = ctx.palette.getColor(hueA, br);

        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            const uint8_t hueB = (uint8_t)(hueA + 64u);
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPCausticShardsEffect
uint8_t LGPCausticShardsEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPCausticShardsEffectParameters) / sizeof(kLGPCausticShardsEffectParameters[0]));
}

const plugins::EffectParameter* LGPCausticShardsEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPCausticShardsEffectParameters[index];
}

bool LGPCausticShardsEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpcaustic_shards_effect_speed_scale") == 0) {
        gLGPCausticShardsEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpcaustic_shards_effect_output_gain") == 0) {
        gLGPCausticShardsEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpcaustic_shards_effect_centre_bias") == 0) {
        gLGPCausticShardsEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPCausticShardsEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpcaustic_shards_effect_speed_scale") == 0) return gLGPCausticShardsEffectSpeedScale;
    if (strcmp(name, "lgpcaustic_shards_effect_output_gain") == 0) return gLGPCausticShardsEffectOutputGain;
    if (strcmp(name, "lgpcaustic_shards_effect_centre_bias") == 0) return gLGPCausticShardsEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPCausticShardsEffect

void LGPCausticShardsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPCausticShardsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Caustic Shards",
        "Interference with prismatic glints",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
