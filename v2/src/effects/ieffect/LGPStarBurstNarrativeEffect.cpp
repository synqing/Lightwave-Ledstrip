// LGPStarBurstNarrativeEffect.cpp
/**
 * @file LGPStarBurstNarrativeEffect.cpp
 * @brief LGP Star Burst effect implementation (narrative conductor + coherent color/motion)
 */

#include "LGPStarBurstNarrativeEffect.h"
#include "../CoreEffects.h"      // STRIP_LENGTH, HALF_LENGTH, centerPairDistance(...)
#include "../../config/features.h"
#include <FastLED.h>             // fadeToBlackBy
#include <cmath>                 // sinf, expf, fabsf, fmodf, roundf

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ------------------------------
// Small math helpers (no malloc)
// ------------------------------

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline uint8_t clampU8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

// Asymmetric one-pole smoother:
// - fast rise, slower fall (human-perceptual friendly)
static float smoothValue(float current, float target, float rise, float fall) {
    const float alpha = (target > current) ? rise : fall;
    return current + (target - current) * alpha;
}

// 0..1 smoothstep with explicit duration
static inline float smoothstepDur(float t, float duration) {
    if (duration <= 0.0f) return 1.0f;
    float x = clamp01(t / duration);
    return x * x * (3.0f - 2.0f * x);
}

// Exponential decay toward 0 with time constant tau (seconds)
static inline float expDecay(float value, float dt, float tauSeconds) {
    if (tauSeconds <= 0.0f) return 0.0f;
    return value * expf(-dt / tauSeconds);
}

LGPStarBurstNarrativeEffect::LGPStarBurstNarrativeEffect()
    : m_phase(0.0f)
{
}

bool LGPStarBurstNarrativeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset story conductor
    m_storyPhase   = StoryPhase::REST;
    m_storyTimeS   = 0.0f;
    m_quietTimeS   = 0.0f;
    m_phraseHoldS  = 10.0f; // allow immediate first commit

    // Reset key/palette gating
    m_candidateRootBin = 0;
    m_candidateMinor   = false;
    m_keyRootBin       = 0;
    m_keyMinor         = false;
    m_keyRootBinSmooth = 0.0f;

    // Reset audio features
    m_phase = 0.0f;
    m_burst = 0.0f;
    m_lastHopSeq = 0;

    m_chromaEnergySum = 0.0f;
    m_chromaHistIdx = 0;
    for (uint8_t i = 0; i < CHROMA_HISTORY; ++i) {
        m_chromaEnergyHist[i] = 0.0f;
    }

    m_energyAvg = 0.0f;
    m_energyDelta = 0.0f;
    m_dominantBin = 0;

    m_energyAvgSmooth = 0.0f;
    m_energyDeltaSmooth = 0.0f;
    m_dominantBinSmooth = 0.0f;

    return true;
}

void LGPStarBurstNarrativeEffect::render(plugins::EffectContext& ctx) {
    // ------------------------------
    // Normalize controls + time step
    // ------------------------------
    const float speedNorm = ctx.speed / 50.0f;         // typical 0..~2
    const float intensityNorm = ctx.brightness / 255.0f;
    const float dt = ctx.deltaTimeMs * 0.001f;         // seconds

    const bool hasAudio = ctx.audio.available;
#if FEATURE_AUDIO_SYNC
    const bool newHop = hasAudio && (ctx.audio.controlBus.hop_seq != m_lastHopSeq);

    // -----------------------------------------
    // AUDIO FEATURE UPDATE (hop-gated)
    // -----------------------------------------
    if (hasAudio && newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;

        // Transform chroma into a stable "brightness proxy"
        float maxBinVal = 0.0f;
        uint8_t dominantBin = 0;

        float chromaEnergyMean = 0.0f; // mean of 12 bins after transform (0..1)
        for (uint8_t i = 0; i < 12; ++i) {
            const float bin = ctx.audio.controlBus.chroma[i];
            float bright = bin * bin;     // emphasize strong notes
            bright *= 1.5f;
            bright = clamp01(bright);

            if (bright > maxBinVal) {
                maxBinVal = bright;
                dominantBin = i;
            }
            chromaEnergyMean += bright;
        }
        chromaEnergyMean *= (1.0f / 12.0f); // mean

        // Candidate tonal center follows dominant bin, but we do NOT immediately commit it.
        const uint8_t minorThirdBin = (uint8_t)((dominantBin + 3) % 12);
        const uint8_t majorThirdBin = (uint8_t)((dominantBin + 4) % 12);
        const float minorThird = ctx.audio.controlBus.chroma[minorThirdBin];
        const float majorThird = ctx.audio.controlBus.chroma[majorThirdBin];
        m_candidateRootBin = dominantBin;
        m_candidateMinor = (minorThird > majorThird);

        // 4-hop moving baseline to compute novelty (energyDelta)
        m_chromaEnergySum -= m_chromaEnergyHist[m_chromaHistIdx];
        m_chromaEnergyHist[m_chromaHistIdx] = chromaEnergyMean;
        m_chromaEnergySum += chromaEnergyMean;
        m_chromaHistIdx = (uint8_t)((m_chromaHistIdx + 1) % CHROMA_HISTORY);

        m_energyAvg = m_chromaEnergySum / (float)CHROMA_HISTORY;

        m_energyDelta = chromaEnergyMean - m_energyAvg;
        if (m_energyDelta < 0.0f) m_energyDelta = 0.0f;

        m_dominantBin = dominantBin;

        // Impact trigger: novelty spike => "story beat"
        if (m_energyDelta > 0.05f) {
            m_burst = 1.0f;
        }
    } else
#endif
    if (!hasAudio) {
        // No audio: decay toward calm
        m_energyAvg *= 0.98f;
        m_energyDelta = 0.0f;
    }

    // -----------------------------------------
    // SMOOTHING (dt-aware, asymmetric)
    // -----------------------------------------
    const float riseAvg  = dt / (0.20f + dt);
    const float fallAvg  = dt / (0.50f + dt);
    const float riseDel  = dt / (0.08f + dt);
    const float fallDel  = dt / (0.25f + dt);
    const float alphaBin = dt / (0.25f + dt);

    m_energyAvgSmooth   = smoothValue(m_energyAvgSmooth,   m_energyAvg,   riseAvg, fallAvg);
    m_energyDeltaSmooth = smoothValue(m_energyDeltaSmooth, m_energyDelta, riseDel, fallDel);

    m_dominantBinSmooth += ((float)m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    // -----------------------------------------
    // STORY CONDUCTOR UPDATE
    // -----------------------------------------
    m_storyTimeS  += dt;
    m_phraseHoldS += dt;

    const bool quietNow = (m_energyAvgSmooth < 0.08f) && (m_energyDeltaSmooth < 0.015f);
    if (quietNow) m_quietTimeS += dt;
    else          m_quietTimeS = 0.0f;

    // REST -> BUILD: wake up (quiet->active) => commit palette/key (phrase gate)
    // BUILD -> HOLD: sustained energy
    // HOLD -> RELEASE: energy drops or quiet persists
    // RELEASE -> REST: quiet persists
    switch (m_storyPhase) {
        case StoryPhase::REST:
            if (!quietNow && m_phraseHoldS > 0.6f) {
                if (m_phraseHoldS > 2.0f) {
                    m_keyRootBin = m_candidateRootBin;
                    m_keyMinor   = m_candidateMinor;
                    m_phraseHoldS = 0.0f;
                }
                m_storyPhase = StoryPhase::BUILD;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::BUILD:
            if (m_quietTimeS > 0.5f) {
                m_storyPhase = StoryPhase::REST;
                m_storyTimeS = 0.0f;
                break;
            }
            if (m_storyTimeS > 1.2f && m_energyAvgSmooth > 0.20f) {
                m_storyPhase = StoryPhase::HOLD;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::HOLD:
            if (m_quietTimeS > 0.6f) {
                m_storyPhase = StoryPhase::RELEASE;
                m_storyTimeS = 0.0f;
                break;
            }
            if (m_storyTimeS > 3.0f && m_energyAvgSmooth < 0.18f) {
                m_storyPhase = StoryPhase::RELEASE;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::RELEASE:
            if (m_quietTimeS > 0.8f) {
                m_storyPhase = StoryPhase::REST;
                m_storyTimeS = 0.0f;
                break;
            }
            if (m_storyTimeS > 1.0f && !quietNow) {
                m_storyPhase = StoryPhase::BUILD;
                m_storyTimeS = 0.0f;
            }
            break;
    }

    // Smooth committed root bin so hue drift feels intentional (still phrase-gated)
    m_keyRootBinSmooth += ((float)m_keyRootBin - m_keyRootBinSmooth) * (dt / (0.35f + dt));
    if (m_keyRootBinSmooth < 0.0f) m_keyRootBinSmooth = 0.0f;
    if (m_keyRootBinSmooth > 11.0f) m_keyRootBinSmooth = 11.0f;

    // Story envelope 0..1
    float env = 0.0f;
    if (m_storyPhase == StoryPhase::REST) {
        env = 0.0f;
    } else if (m_storyPhase == StoryPhase::BUILD) {
        env = smoothstepDur(m_storyTimeS, 1.2f);
    } else if (m_storyPhase == StoryPhase::HOLD) {
        env = 1.0f;
    } else { // RELEASE
        env = 1.0f - smoothstepDur(m_storyTimeS, 1.0f);
    }
    env = clamp01(env);

    // -----------------------------------------
    // PHASE + IMPACT DYNAMICS (dt-correct)
    // -----------------------------------------
    const float speedScale =
        (0.45f + 1.10f * m_energyAvgSmooth + 2.20f * m_energyDeltaSmooth) * (0.25f + 0.75f * env);

    m_phase += (speedNorm * 2.2f) * speedScale * dt; // radians
    if (m_phase > 100000.0f) m_phase = fmodf(m_phase, 6.2831853f);

    m_burst += m_energyDeltaSmooth * 0.85f;
    if (m_burst > 1.0f) m_burst = 1.0f;
    m_burst = expDecay(m_burst, dt, 0.18f); // ~180ms impact tail

    // -----------------------------------------
    // TRAILS (dt-correct)
    // -----------------------------------------
    const int fadeAmt = (int)roundf(20.0f * (dt * 60.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, clampU8(fadeAmt));

    // -----------------------------------------
    // RENDER (center-origin, coherent color + motion)
    // -----------------------------------------
    const uint8_t rootBin  = (uint8_t)roundf(m_keyRootBinSmooth);
    const uint8_t thirdBin = (uint8_t)((rootBin + (m_keyMinor ? 3 : 4)) % 12);
    const uint8_t fifthBin = (uint8_t)((rootBin + 7) % 12);

    const uint8_t binStep  = (uint8_t)(255.0f / 12.0f);
    const uint8_t hueRoot  = (uint8_t)(ctx.gHue + rootBin  * binStep);
    const uint8_t hueThird = (uint8_t)(ctx.gHue + thirdBin * binStep);
    const uint8_t hueFifth = (uint8_t)(ctx.gHue + fifthBin * binStep);

    const float freqBase   = 0.18f + 0.30f * env;
    const float falloff    = 3.2f  - 1.6f  * env;
    const float pulseRate  = 0.8f  + 2.4f  * env;

    const uint8_t motionIdx = (uint8_t)(((uint32_t)(m_phase * 40.0f)) & 0xFF);

    const int maxLeds = (ctx.ledCount < STRIP_LENGTH) ? (int)ctx.ledCount : (int)STRIP_LENGTH;

    for (int i = 0; i < maxLeds; i++) {
        const float distFromCenter = (float)centerPairDistance((uint16_t)i);
        const float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        const float ray1 = sinf(distFromCenter * freqBase - m_phase);
        const float ray2 = 0.35f * env * sinf(distFromCenter * (freqBase * 2.0f) - (m_phase * 1.3f));

        const float spatial = expf(-normalizedDist * falloff);
        const float pulse   = 0.35f + 0.65f * (0.5f + 0.5f * sinf(m_phase * pulseRate));

        float field = 0.5f + 0.5f * (ray1 + ray2);
        field *= spatial;
        field *= pulse;

        if (env > 0.02f) {
            field += m_burst * env * expf(-normalizedDist * (falloff + 0.6f));
        }

        field = clamp01(field);
        field *= field; // contrast

        float brightF = field;
        if (m_storyPhase == StoryPhase::REST) {
            brightF *= 0.20f; // real rest
        } else if (m_storyPhase == StoryPhase::BUILD) {
            brightF *= (0.35f + 0.65f * env);
        }

        const uint8_t brightness = clampU8((int)roundf(brightF * intensityNorm * 255.0f));

        const int baseIdx = (int)(distFromCenter * 2.0f) + (int)motionIdx;
        const uint8_t paletteIndex = clampU8(baseIdx);

        const float t = clamp01(normalizedDist);

        float wRoot  = clamp01(1.15f - 1.65f * t);
        float wFifth = clamp01(0.35f + 0.95f * t);
        float wThird = env * clamp01(1.0f - fabsf(t - 0.35f) * 3.0f);

        const float wSum = (wRoot + wThird + wFifth);
        if (wSum > 0.0001f) {
            wRoot  /= wSum;
            wThird /= wSum;
            wFifth /= wSum;
        }

        const uint8_t bRoot  = clampU8((int)roundf(brightness * wRoot));
        const uint8_t bThird = clampU8((int)roundf(brightness * wThird));
        const uint8_t bFifth = clampU8((int)roundf(brightness * wFifth));

        CRGB c = CRGB::Black;
        if (bRoot)  c += ctx.palette.getColor((uint8_t)(hueRoot  + paletteIndex), bRoot);
        if (bThird) c += ctx.palette.getColor((uint8_t)(hueThird + paletteIndex), bThird);
        if (bFifth) c += ctx.palette.getColor((uint8_t)(hueFifth + paletteIndex), bFifth);

        if (m_burst > 0.20f && env > 0.25f) {
            const uint8_t accentB = clampU8((int)roundf(brightness * m_burst * 0.55f));
            c += ctx.palette.getColor((uint8_t)(hueRoot + 128 + paletteIndex), accentB);
        }

        ctx.leds[i] += c;

        if (i + STRIP_LENGTH < ctx.ledCount) {
            const uint8_t harmonyShift = 85;

            CRGB c2 = CRGB::Black;
            if (bRoot)  c2 += ctx.palette.getColor((uint8_t)(hueRoot  + harmonyShift + paletteIndex), bRoot);
            if (bThird) c2 += ctx.palette.getColor((uint8_t)(hueThird + harmonyShift + paletteIndex), bThird);
            if (bFifth) c2 += ctx.palette.getColor((uint8_t)(hueFifth + harmonyShift + paletteIndex), bFifth);

            if (m_burst > 0.20f && env > 0.25f) {
                const uint8_t accentB = clampU8((int)roundf(brightness * m_burst * 0.55f));
                c2 += ctx.palette.getColor((uint8_t)(hueRoot + harmonyShift + 128 + paletteIndex), accentB);
            }

            ctx.leds[i + STRIP_LENGTH] += c2;
        }
    }
}

void LGPStarBurstNarrativeEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStarBurstNarrativeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Star Burst (Narrative)",
        "Center-origin starburst with story conductor + chord-based color",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
