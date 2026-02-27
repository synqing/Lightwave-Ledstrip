/**
 * @file LGPReactionDiffusionTestRigEffect.cpp
 * @brief LGP Reaction Diffusion Test Rig effect implementation
 */

#include "LGPReactionDiffusionTestRigEffect.h"
#include "../CoreEffects.h"
#include "../../utils/Log.h"
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

LGPReactionDiffusionTestRigEffect::LGPReactionDiffusionTestRigEffect()
    : m_lastMode(255)
    , m_frame(0)
{
}

bool LGPReactionDiffusionTestRigEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Allocate large buffers in PSRAM (DRAM is too precious)
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPReactionDiffusionTestRigEffect: PSRAM alloc failed (%u bytes)", (unsigned)sizeof(PsramData));
            return false;
        }
    }

    m_frame = 0;
    m_lastMode = 255;
    resetForMode(0);
    return true;
}

void LGPReactionDiffusionTestRigEffect::resetForMode(uint8_t mode) {
    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->u[i] = 1.0f;
        m_ps->v[i] = 0.0f;
    }

    const int mid = STRIP_LENGTH / 2;

    if (mode == 9) {
        m_ps->v[mid] = 1.0f;
        m_ps->u[mid] = 0.0f;
    } else if (mode == 10) {
        for (int i = mid - 12; i <= mid + 12; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                m_ps->v[i] = 1.0f;
                m_ps->u[i] = 0.0f;
            }
        }
    } else if (mode == 11) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            const float n = (float)((hash32((uint32_t)i) & 1023u)) / 1023.0f;
            m_ps->v[i] = 0.08f * n;
            m_ps->u[i] = 1.0f - 0.25f * m_ps->v[i];
        }
        for (int i = mid - 6; i <= mid + 6; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                m_ps->v[i] = 1.0f;
                m_ps->u[i] = 0.0f;
            }
        }
    } else {
        for (int i = mid - 6; i <= mid + 6; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                m_ps->v[i] = 1.0f;
                m_ps->u[i] = 0.0f;
            }
        }
    }
}

void LGPReactionDiffusionTestRigEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;

    uint8_t mode = (uint8_t)(ctx.gHue >> 4);
    if (mode > 11) mode = 0;

    if (mode != m_lastMode) {
        resetForMode(mode);
        m_lastMode = mode;
        m_frame = 0;
    }

    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    const float Du = 1.0f;
    const float Dv = 0.5f;
    const float F = 0.0380f;
    const float K = 0.0630f;

    const float dt = 0.9f + 0.6f * speedNorm;
    const int iters = (speedNorm > 0.55f) ? 2 : 1;

    enum { BC_CLAMP = 0, BC_PERIODIC = 1, BC_REFLECT = 2 };
    int bc = BC_CLAMP;
    if (mode == 6 || mode == 7) bc = BC_PERIODIC;
    if (mode == 8) bc = BC_REFLECT;

    for (int iter = 0; iter < iters; iter++) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            int im1;
            int ip1;
            if (bc == BC_PERIODIC) {
                im1 = (i == 0) ? (STRIP_LENGTH - 1) : (i - 1);
                ip1 = (i == STRIP_LENGTH - 1) ? 0 : (i + 1);
            } else if (bc == BC_REFLECT) {
                im1 = (i == 0) ? 1 : (i - 1);
                ip1 = (i == STRIP_LENGTH - 1) ? (STRIP_LENGTH - 2) : (i + 1);
            } else {
                im1 = (i == 0) ? 0 : (i - 1);
                ip1 = (i == STRIP_LENGTH - 1) ? (STRIP_LENGTH - 1) : (i + 1);
            }

            const float lapU = m_ps->u[im1] - 2.0f * m_ps->u[i] + m_ps->u[ip1];
            const float lapV = m_ps->v[im1] - 2.0f * m_ps->v[i] + m_ps->v[ip1];

            const float u = m_ps->u[i];
            const float v = m_ps->v[i];
            const float uvv = u * v * v;

            m_ps->u2[i] = u + (Du * lapU - uvv + F * (1.0f - u)) * dt;
            m_ps->v2[i] = v + (Dv * lapV + uvv - (K + F) * v) * dt;

            m_ps->u2[i] = clamp01(m_ps->u2[i]);
            m_ps->v2[i] = clamp01(m_ps->v2[i]);
        }

        for (int i = 0; i < STRIP_LENGTH; i++) {
            m_ps->u[i] = m_ps->u2[i];
            m_ps->v[i] = m_ps->v2[i];
        }
    }

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const bool showEdges = (mode == 4 || mode == 5);
    const float T = 0.25f;

    int leftEdge = -1;
    int rightEdge = -1;
    if (showEdges) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            if (m_ps->v[i] > T) { leftEdge = i; break; }
        }
        for (int i = STRIP_LENGTH - 1; i >= 0; i--) {
            if (m_ps->v[i] > T) { rightEdge = i; break; }
        }
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = (float)i;
        const float dmid = x - mid;

        float env = 1.0f;
        if (mode == 0 || mode == 1) {
            env = expf(-(dmid * dmid) * 0.0018f);
        } else if (mode == 3 || mode == 5 || mode == 7 || mode == 9 || mode == 10 || mode == 11) {
            const float halfWidth = 55.0f;
            env = 1.0f - (fabsf(dmid) / halfWidth);
            env = clamp01(env);
        } else {
            env = 1.0f;
        }

        const float v = m_ps->v[i];

        if (mode == 0) {
            const float dist = (float)centerPairDistance((uint16_t)i);
            float wave = clamp01(0.15f * env + 0.85f * (v * env + 0.25f * v));
            const float base = 0.07f;
            const float out = clamp01(base + (1.0f - base) * wave) * master;
            const uint8_t br = (uint8_t)(255.0f * out);
            const uint8_t hue = (uint8_t)(ctx.gHue + (int)(dist * 0.6f) + (int)(v * 180.0f));
            ctx.leds[i] = ctx.palette.getColor(hue, br);
        } else {
            float wave = clamp01(v * env);
            const float base = 0.06f;
            const float out = clamp01(base + (1.0f - base) * wave) * master;
            const uint8_t br = (uint8_t)(255.0f * out);
            const uint8_t hue = (uint8_t)(ctx.gHue);
            ctx.leds[i] = ctx.palette.getColor(hue, br);
        }

        if (showEdges) {
            if (i == (int)mid) {
                ctx.leds[i] = CRGB(80, 80, 80);
            }
            if (i == leftEdge || i == rightEdge) {
                ctx.leds[i] = CRGB::White;
            }
        }

        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            CRGB c = ctx.leds[i];
            c.nscale8_video(245);
            ctx.leds[j] = c;
        }
    }

    m_frame++;
}

void LGPReactionDiffusionTestRigEffect::cleanup() {
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
}

const plugins::EffectMetadata& LGPReactionDiffusionTestRigEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP RD Test Rig",
        "Gray-Scott diagnostic matrix (12 modes via Hue>>4)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
