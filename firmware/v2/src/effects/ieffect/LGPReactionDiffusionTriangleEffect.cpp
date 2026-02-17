/**
 * @file LGPReactionDiffusionTriangleEffect.cpp
 * @brief LGP Reaction Diffusion (tunable) effect implementation
 */

#include "LGPReactionDiffusionTriangleEffect.h"
#include "../CoreEffects.h"
#include "../../utils/Log.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPReactionDiffusionTriangleEffect::LGPReactionDiffusionTriangleEffect()
    : m_t(0.0f)
    , m_paramF(0.0380f)
    , m_paramK(0.0630f)
    , m_paramMeltK(0.0018f)
{
}

bool LGPReactionDiffusionTriangleEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_t = 0.0f;

    // Allocate large buffers in PSRAM (DRAM is too precious)
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPReactionDiffusionTriangleEffect: PSRAM alloc failed (%u bytes)", (unsigned)sizeof(PsramData));
            return false;
        }
    }

    // Reset tunables to defaults for a baseline-identical start.
    m_paramF = 0.0380f;
    m_paramK = 0.0630f;
    m_paramMeltK = 0.0018f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->u[i] = 1.0f;
        m_ps->v[i] = 0.0f;
    }

    const int mid = STRIP_LENGTH / 2;
    for (int i = mid - 6; i <= mid + 6; i++) {
        if (i >= 0 && i < STRIP_LENGTH) {
            m_ps->v[i] = 1.0f;
            m_ps->u[i] = 0.0f;
        }
    }

    return true;
}

void LGPReactionDiffusionTriangleEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;

    // Identical to LGPReactionDiffusionEffect, with effect-local tunable levers (default = baseline).
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    // Gray-Scott parameters (baseline pocket, with small tunable offsets)
    const float Du = 1.0f;
    const float Dv = 0.5f;
    const float F = m_paramF;
    const float K = m_paramK;

    const float dt = 0.9f + 0.6f * speedNorm;
    const int iters = (speedNorm > 0.55f) ? 2 : 1;

    for (int iter = 0; iter < iters; iter++) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            const int im1 = (i == 0) ? 0 : (i - 1);
            const int ip1 = (i == STRIP_LENGTH - 1) ? (STRIP_LENGTH - 1) : (i + 1);

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

    // Render: map V concentration to brightness and hue; add centre “melt glue”.
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float meltK = m_paramMeltK;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = (float)i;
        const float dist = (float)centerPairDistance((uint16_t)i);

        const float dmid = x - mid;
        const float melt = expf(-(dmid * dmid) * meltK);

        const float v = m_ps->v[i];
        float wave = clamp01(0.15f * melt + 0.85f * (v * melt + 0.25f * v));

        const float base = 0.07f;
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t brA = (uint8_t)(255.0f * out);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(dist * 0.6f) + (int)(v * 180.0f));

        const uint8_t hueB = (uint8_t)(hueA + 4u);
        const uint8_t brB = scale8_video(brA, 245);

        ctx.leds[i] = ctx.palette.getColor(hueA, brA);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }

    m_t += 1.0f;
}

void LGPReactionDiffusionTriangleEffect::cleanup() {
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
}

const plugins::EffectMetadata& LGPReactionDiffusionTriangleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP RD Triangle",
        "Reaction-diffusion (tunable levers)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPReactionDiffusionTriangleEffect::getParameterCount() const {
    return 3;
}

const plugins::EffectParameter* LGPReactionDiffusionTriangleEffect::getParameter(uint8_t index) const {
    static plugins::EffectParameter params[] = {
        plugins::EffectParameter("F", "Feed (F)", 0.0300f, 0.0500f, 0.0380f),
        plugins::EffectParameter("K", "Kill (K)", 0.0550f, 0.0750f, 0.0630f),
        plugins::EffectParameter("melt", "Melt Glue", 0.0010f, 0.0035f, 0.0018f)
    };
    if (index >= (sizeof(params) / sizeof(params[0]))) {
        return nullptr;
    }
    return &params[index];
}

bool LGPReactionDiffusionTriangleEffect::setParameter(const char* name, float value) {
    if (!name) return false;

    if (strcmp(name, "F") == 0) {
        if (value < 0.0300f) value = 0.0300f;
        if (value > 0.0500f) value = 0.0500f;
        m_paramF = value;
        return true;
    }
    if (strcmp(name, "K") == 0) {
        if (value < 0.0550f) value = 0.0550f;
        if (value > 0.0750f) value = 0.0750f;
        m_paramK = value;
        return true;
    }
    if (strcmp(name, "melt") == 0) {
        if (value < 0.0010f) value = 0.0010f;
        if (value > 0.0035f) value = 0.0035f;
        m_paramMeltK = value;
        return true;
    }
    return false;
}

float LGPReactionDiffusionTriangleEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "F") == 0) return m_paramF;
    if (strcmp(name, "K") == 0) return m_paramK;
    if (strcmp(name, "melt") == 0) return m_paramMeltK;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
