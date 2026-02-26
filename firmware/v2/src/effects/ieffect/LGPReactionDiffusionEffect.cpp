/**
 * @file LGPReactionDiffusionEffect.cpp
 * @brief LGP Reaction Diffusion effect implementation
 */

#include "LGPReactionDiffusionEffect.h"
#include "../CoreEffects.h"
#include "../../utils/Log.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPReactionDiffusionEffect
namespace {
constexpr float kLGPReactionDiffusionEffectSpeedScale = 1.0f;
constexpr float kLGPReactionDiffusionEffectOutputGain = 1.0f;
constexpr float kLGPReactionDiffusionEffectCentreBias = 1.0f;

float gLGPReactionDiffusionEffectSpeedScale = kLGPReactionDiffusionEffectSpeedScale;
float gLGPReactionDiffusionEffectOutputGain = kLGPReactionDiffusionEffectOutputGain;
float gLGPReactionDiffusionEffectCentreBias = kLGPReactionDiffusionEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPReactionDiffusionEffectParameters[] = {
    {"lgpreaction_diffusion_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPReactionDiffusionEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpreaction_diffusion_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPReactionDiffusionEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpreaction_diffusion_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPReactionDiffusionEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPReactionDiffusionEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPReactionDiffusionEffect::LGPReactionDiffusionEffect()
    : m_t(0.0f)
{
}

bool LGPReactionDiffusionEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPReactionDiffusionEffect
    gLGPReactionDiffusionEffectSpeedScale = kLGPReactionDiffusionEffectSpeedScale;
    gLGPReactionDiffusionEffectOutputGain = kLGPReactionDiffusionEffectOutputGain;
    gLGPReactionDiffusionEffectCentreBias = kLGPReactionDiffusionEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPReactionDiffusionEffect

    m_t = 0.0f;

    // Allocate large buffers in PSRAM (DRAM is too precious)
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPReactionDiffusionEffect: PSRAM alloc failed (%u bytes)", (unsigned)sizeof(PsramData));
            return false;
        }
    }

    // Standard init: U=1 everywhere, V=0; seed a small region of V.
    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->u[i] = 1.0f;
        m_ps->v[i] = 0.0f;
    }
    const int mid = STRIP_LENGTH / 2;
    for (int i = mid - 6; i <= mid + 6; i++) {
        if (i >= 0 && i < STRIP_LENGTH) { m_ps->v[i] = 1.0f; m_ps->u[i] = 0.0f; }
    }

    return true;
}

void LGPReactionDiffusionEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;

    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    // --- Gray-Scott parameters ---
    const float Du = 1.0f;
    const float Dv = 0.5f;
    const float F = 0.0380f;
    const float K = 0.0630f;

    // Time step: faster at higher speed, but keep stable.
    const float dt = 0.9f + 0.6f * speedNorm;

    // Do 1–2 iterations per frame depending on speed (keeps cost bounded).
    const int iters = (speedNorm > 0.55f) ? 2 : 1;

    for (int iter = 0; iter < iters; iter++) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            const int im1 = (i == 0) ? 0 : (i - 1);
            const int ip1 = (i == STRIP_LENGTH - 1) ? (STRIP_LENGTH - 1) : (i + 1);

            // 1D Laplacian (simple and stable for this use).
            const float lapU = m_ps->u[im1] - 2.0f * m_ps->u[i] + m_ps->u[ip1];
            const float lapV = m_ps->v[im1] - 2.0f * m_ps->v[i] + m_ps->v[ip1];

            const float u = m_ps->u[i];
            const float v = m_ps->v[i];

            // Gray-Scott reaction term (u*v^2) and feed/kill.
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
    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = (float)i;
        const float dist = (float)centerPairDistance((uint16_t)i);

        const float dmid = x - mid;
        const float melt = expf(-(dmid * dmid) * 0.0018f);

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


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPReactionDiffusionEffect
uint8_t LGPReactionDiffusionEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPReactionDiffusionEffectParameters) / sizeof(kLGPReactionDiffusionEffectParameters[0]));
}

const plugins::EffectParameter* LGPReactionDiffusionEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPReactionDiffusionEffectParameters[index];
}

bool LGPReactionDiffusionEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpreaction_diffusion_effect_speed_scale") == 0) {
        gLGPReactionDiffusionEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpreaction_diffusion_effect_output_gain") == 0) {
        gLGPReactionDiffusionEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpreaction_diffusion_effect_centre_bias") == 0) {
        gLGPReactionDiffusionEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPReactionDiffusionEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpreaction_diffusion_effect_speed_scale") == 0) return gLGPReactionDiffusionEffectSpeedScale;
    if (strcmp(name, "lgpreaction_diffusion_effect_output_gain") == 0) return gLGPReactionDiffusionEffectOutputGain;
    if (strcmp(name, "lgpreaction_diffusion_effect_centre_bias") == 0) return gLGPReactionDiffusionEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPReactionDiffusionEffect

void LGPReactionDiffusionEffect::cleanup() {
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
}

const plugins::EffectMetadata& LGPReactionDiffusionEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Reaction Diffusion",
        "Gray-Scott 1D slime",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
