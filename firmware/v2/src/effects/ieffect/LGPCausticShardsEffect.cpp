/**
 * @file LGPCausticShardsEffect.cpp
 * @brief LGP Caustic Shards effect implementation
 */

#include "LGPCausticShardsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstdint>

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
