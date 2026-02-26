/**
 * @file LGPGravitationalWaveChirpEffect.cpp
 * @brief LGP Gravitational Wave Chirp effect implementation
 */

#include "LGPGravitationalWaveChirpEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kChirpBase = 0.002f;
constexpr float kChirpScale = 0.008f;
constexpr float kMergeDecay = 0.92f;

const plugins::EffectParameter kParameters[] = {
    {"chirp_base", "Chirp Base", 0.0005f, 0.01f, kChirpBase, plugins::EffectParameterType::FLOAT, 0.0005f, "timing", "x", false},
    {"chirp_scale", "Chirp Scale", 0.001f, 0.02f, kChirpScale, plugins::EffectParameterType::FLOAT, 0.0005f, "timing", "x", false},
    {"merge_decay", "Merge Decay", 0.80f, 0.98f, kMergeDecay, plugins::EffectParameterType::FLOAT, 0.005f, "blend", "", false},
};
}

LGPGravitationalWaveChirpEffect::LGPGravitationalWaveChirpEffect()
    : m_inspiralProgress(0.0f)
    , m_ringdownPhase(0.0f)
    , m_merging(false)
    , m_ringdown(false)
    , m_mergeFlash(0.0f)
    , m_phase1(0.0f)
    , m_phase2(0.0f)
    , m_chirpBase(kChirpBase)
    , m_chirpScale(kChirpScale)
    , m_mergeDecay(kMergeDecay)
{
}

bool LGPGravitationalWaveChirpEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_inspiralProgress = 0.0f;
    m_ringdownPhase = 0.0f;
    m_merging = false;
    m_ringdown = false;
    m_mergeFlash = 0.0f;
    m_phase1 = 0.0f;
    m_phase2 = 0.0f;
    m_chirpBase = kChirpBase;
    m_chirpScale = kChirpScale;
    m_mergeDecay = kMergeDecay;
    return true;
}

void LGPGravitationalWaveChirpEffect::render(plugins::EffectContext& ctx) {
    // Binary black hole inspiral with LIGO-accurate frequency evolution
    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    float chirpRate = m_chirpBase + speed * m_chirpScale;
    const float massRatio = 1.0f;

    if (!m_merging && !m_ringdown) {
        m_inspiralProgress += chirpRate;

        if (m_inspiralProgress >= 1.0f) {
            m_merging = true;
            m_mergeFlash = 1.0f;
        }
    } else if (m_merging) {
        m_mergeFlash *= powf(m_mergeDecay, dt * 60.0f);
        if (m_mergeFlash < 0.05f) {
            m_merging = false;
            m_ringdown = true;
            m_ringdownPhase = 0.0f;
        }
    } else if (m_ringdown) {
        m_ringdownPhase += 0.15f + 0.1f;
        float ringdownDecay = expf(-m_ringdownPhase * 0.05f);

        if (ringdownDecay < 0.01f) {
            m_ringdown = false;
            m_inspiralProgress = 0.0f;
        }
    }

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float wave1 = 0.0f;
        float wave2 = 0.0f;

        if (!m_merging && !m_ringdown) {
            float t_remaining = fmaxf(0.01f, 1.0f - m_inspiralProgress);
            float chirpFreq = powf(t_remaining, -3.0f / 8.0f * massRatio);
            chirpFreq = constrain(chirpFreq, 1.0f, 20.0f);

            float amplitude = intensity * (1.0f + m_inspiralProgress * 2.0f);

            m_phase1 += chirpFreq * 0.1f;
            m_phase2 = m_phase1 + PI / 2.0f;

            float compressionFactor = 1.0f + m_inspiralProgress * 3.0f;
            float spatialPhase = normalizedDist * chirpFreq * compressionFactor;

            wave1 = sinf(spatialPhase - m_phase1) * amplitude * (1.0f - normalizedDist);
            wave2 = sinf(spatialPhase - m_phase2) * amplitude * (1.0f - normalizedDist);

        } else if (m_merging) {
            float flashRadius = 0.3f + (1.0f - m_mergeFlash) * 0.5f;
            if (normalizedDist < flashRadius) {
                float flashIntensity = m_mergeFlash * (1.0f - normalizedDist / flashRadius);
                wave1 = flashIntensity * intensity * 2.0f;
                wave2 = flashIntensity * intensity * 2.0f;
            }

        } else if (m_ringdown) {
            const float ringdownFreq = 10.0f;
            float ringdownDecay = expf(-m_ringdownPhase * 0.05f);
            float ringRadius = m_ringdownPhase * 0.1f;

            float distToRing = fabsf(normalizedDist - fmodf(ringRadius, 1.0f));
            if (distToRing < 0.2f) {
                float ringShape = cosf(distToRing / 0.2f * PI / 2.0f);
                wave1 = sinf(m_ringdownPhase * ringdownFreq) * ringShape * ringdownDecay * intensity;
                wave2 = cosf(m_ringdownPhase * ringdownFreq) * ringShape * ringdownDecay * intensity;
            }
        }

        uint8_t brightness1 = (uint8_t)(128 + constrain((int)(wave1 * 127.0f), -127, 127));
        uint8_t brightness2 = (uint8_t)(128 + constrain((int)(wave2 * 127.0f), -127, 127));

        uint8_t baseHue = 200;
        if (m_merging) baseHue = 40;
        else if (m_ringdown) baseHue = 160;

        uint8_t brightU8_1 = (uint8_t)((brightness1 * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor((uint8_t)(baseHue + ctx.gHue), brightU8_1);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t brightU8_2 = (uint8_t)((brightness2 * ctx.brightness) / 255);
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(baseHue + ctx.gHue + 30), brightU8_2);
        }
    }
}

void LGPGravitationalWaveChirpEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGravitationalWaveChirpEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Gravitational Wave Chirp",
        "Inspiral merger signal",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPGravitationalWaveChirpEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPGravitationalWaveChirpEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPGravitationalWaveChirpEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "chirp_base") == 0) {
        m_chirpBase = constrain(value, 0.0005f, 0.01f);
        return true;
    }
    if (strcmp(name, "chirp_scale") == 0) {
        m_chirpScale = constrain(value, 0.001f, 0.02f);
        return true;
    }
    if (strcmp(name, "merge_decay") == 0) {
        m_mergeDecay = constrain(value, 0.80f, 0.98f);
        return true;
    }
    return false;
}

float LGPGravitationalWaveChirpEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "chirp_base") == 0) return m_chirpBase;
    if (strcmp(name, "chirp_scale") == 0) return m_chirpScale;
    if (strcmp(name, "merge_decay") == 0) return m_mergeDecay;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
