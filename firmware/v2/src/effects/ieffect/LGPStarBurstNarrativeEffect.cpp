/**
 * @file LGPStarBurstNarrativeEffect.cpp
 * @brief LGP Star Burst (Narrative) - legacy starburst core with phrase-gated harmonic commits
 */

#include "LGPStarBurstNarrativeEffect.h"

#include "../CoreEffects.h"
#include "../../config/features.h"

#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline uint8_t clampU8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<uint8_t>(v);
}

static inline float smoothstep01(float x) {
    float t = clamp01(x);
    return t * t * (3.0f - 2.0f * t);
}

static inline float smoothstepDur(float t, float durationS) {
    if (durationS <= 0.0f) return 1.0f;
    return smoothstep01(t / durationS);
}

static inline float expAlpha(float dt, float tauS) {
    if (tauS <= 0.0f) return 1.0f;
    return 1.0f - expf(-dt / tauS);
}

} // namespace

LGPStarBurstNarrativeEffect::LGPStarBurstNarrativeEffect()
    : m_phase(0.0f) {
}

bool LGPStarBurstNarrativeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    m_storyPhase = StoryPhase::REST;
    m_storyTimeS = 0.0f;
    m_quietTimeS = 0.0f;
    m_phraseHoldS = 2.0f;

    m_phase = 0.0f;
    m_burst = 0.0f;
    m_lastHopSeq = 0;

    m_candidateRootBin = 0;
    m_candidateMinor = false;
    m_keyRootBin = 0;
    m_keyMinor = false;
    m_dominantBin = 0;
    m_keyRootAngle = 0.0f;

    m_phaseSpeedSpring.init(50.0f, 1.0f);
    m_phaseSpeedSpring.reset(1.0f);

    m_heavyBassSmooth = 0.0f;
    m_heavyBassSmoothInitialised = false;
    m_kickEnv = 0.0f;

    return true;
}

void LGPStarBurstNarrativeEffect::render(plugins::EffectContext& ctx) {
    const float userSpeed = clamp01((ctx.speed - 1) / 99.0f);
    const float userMood  = ctx.getMoodNormalized();
    const float intensityNorm = ctx.brightness / 255.0f;
    const float dtAudio = ctx.getSafeRawDeltaSeconds();
    float dtVisual = ctx.getSafeDeltaSeconds();
    if (dtVisual > 0.1f) dtVisual = 0.1f;

    bool hasAudio = false;
#if FEATURE_AUDIO_SYNC
    hasAudio = ctx.audio.available;
#endif

#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        const bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // Both backends now produce normalised chroma via Stage A/B pipeline.
            const float* chroma = ctx.audio.controlBus.chroma;

            float maxBin = 0.0f;
            uint8_t dominant = 0;
            for (uint8_t i = 0; i < 12; ++i) {
                const float v = chroma ? chroma[i] : 0.0f;
                if (v > maxBin) {
                    maxBin = v;
                    dominant = i;
                }
            }
            m_dominantBin = dominant;

            if (ctx.audio.chordConfidence() >= 0.45f) {
                m_candidateRootBin = ctx.audio.rootNote();
                m_candidateMinor = ctx.audio.isMinor();
            } else {
                m_candidateRootBin = dominant;
                m_candidateMinor = m_keyMinor;
            }

            // Phrase-gated key commit keeps colour stable instead of hopping every frame.
            if (m_phraseHoldS > 1.5f &&
                (ctx.audio.chordConfidence() >= 0.55f || m_storyPhase == StoryPhase::REST)) {
                m_keyRootBin = m_candidateRootBin;
                m_keyMinor = m_candidateMinor;
                m_phraseHoldS = 0.0f;
            }
        }

        if (ctx.audio.isSnareHit()) {
            const float burstCeiling = 0.35f + 0.40f * userMood;
            m_burst = fmaxf(m_burst, burstCeiling);
            if (m_storyPhase == StoryPhase::REST) {
                m_storyPhase = StoryPhase::BUILD;
                m_storyTimeS = 0.0f;
            }
        }

        // Kick envelope from beat events
        if (ctx.audio.isOnBeat()) {
            const float kickCeiling = 0.50f + 0.35f * userMood;
            m_kickEnv = fmaxf(m_kickEnv, kickCeiling);
            m_phaseSpeedSpring.velocity += 0.8f + 1.2f * userSpeed;
        }
        const float kickReleaseTau = 0.20f - 0.08f * userMood;
        m_kickEnv *= expf(-dtAudio / kickReleaseTau);

        const float heavyRaw = clamp01(ctx.audio.heavyBass());
        const float tauHeavy = 0.12f - 0.07f * userMood;
        if (!m_heavyBassSmoothInitialised) {
            m_heavyBassSmooth = heavyRaw;
            m_heavyBassSmoothInitialised = true;
        } else {
            const float heavyAlpha = expAlpha(dtAudio, tauHeavy);
            m_heavyBassSmooth += (heavyRaw - m_heavyBassSmooth) * heavyAlpha;
        }
    } else
#endif
    {
        m_heavyBassSmooth *= expf(-dtAudio / 0.25f);
    }

    m_storyTimeS += dtAudio;
    m_phraseHoldS += dtAudio;

    float rmsNow = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        rmsNow = clamp01(ctx.audio.rms());
    }
#endif

    const bool energyQuiet = (rmsNow < 0.06f) && (m_heavyBassSmooth < 0.08f) && (m_burst < 0.06f);
    const bool noRecentKick = (m_kickEnv < 0.05f);
    const bool quietNow = (!hasAudio) || (energyQuiet && noRecentKick);
    if (quietNow) {
        m_quietTimeS += dtAudio;
    } else {
        m_quietTimeS = 0.0f;
    }

    switch (m_storyPhase) {
        case StoryPhase::REST:
            if (!quietNow && m_phraseHoldS > 0.65f) {
                if (m_phraseHoldS > 1.5f) {
                    m_keyRootBin = m_candidateRootBin;
                    m_keyMinor = m_candidateMinor;
                    m_phraseHoldS = 0.0f;
                }
                m_storyPhase = StoryPhase::BUILD;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::BUILD:
            if (m_quietTimeS > (0.80f + 0.40f * userMood)) {
                m_storyPhase = StoryPhase::REST;
                m_storyTimeS = 0.0f;
            } else if (m_storyTimeS > 0.90f && m_heavyBassSmooth > 0.14f) {
                m_storyPhase = StoryPhase::HOLD;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::HOLD:
            if (m_quietTimeS > (0.90f + 0.50f * userMood) ||
                (m_storyTimeS > 2.40f && m_heavyBassSmooth < 0.10f)) {
                m_storyPhase = StoryPhase::RELEASE;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::RELEASE: {
            const float releaseDurS = 1.40f - 0.85f * userMood;
            if (m_quietTimeS > (1.10f + 0.40f * userMood) || m_storyTimeS > releaseDurS) {
                m_storyPhase = StoryPhase::REST;
                m_storyTimeS = 0.0f;
            } else if (!quietNow && m_storyTimeS > 0.70f) {
                m_storyPhase = StoryPhase::BUILD;
                m_storyTimeS = 0.0f;
            }
            break;
        }
    }

    const float buildDurS   = 1.20f - 0.75f * userMood;
    const float releaseDurEnvS = 1.40f - 0.85f * userMood;

    float env = 0.0f;
    switch (m_storyPhase) {
        case StoryPhase::REST:
            env = 0.0f;
            break;
        case StoryPhase::BUILD:
            env = smoothstepDur(m_storyTimeS, buildDurS);
            break;
        case StoryPhase::HOLD:
            env = 1.0f;
            break;
        case StoryPhase::RELEASE:
            env = 1.0f - smoothstepDur(m_storyTimeS, releaseDurEnvS);
            break;
    }
    env = clamp01(env);

    // Circular EMA for key root bin (eliminates rainbow sweep at bin 11->0 boundary)
    {
        const float keyAlpha = expAlpha(dtAudio, 0.35f);
        const float targetAngle = static_cast<float>(m_keyRootBin) *
            (effects::chroma::TWO_PI_F / 12.0f);
        m_keyRootAngle = effects::chroma::circularEma(
            targetAngle, m_keyRootAngle, keyAlpha);
    }

    // Mood-controlled spring dynamics
    m_phaseSpeedSpring.stiffness = 35.0f + 65.0f * userMood;
    m_phaseSpeedSpring.damping = 2.0f * sqrtf(m_phaseSpeedSpring.stiffness * m_phaseSpeedSpring.mass);

    const float minSpeed = 0.25f + 0.55f * userSpeed;
    const float maxSpeed = 1.25f + 1.95f * userSpeed;
    const float targetSpeed = (0.55f + 0.95f * userSpeed)
                            + (0.30f + 1.50f * userSpeed) * m_heavyBassSmooth;
    float smoothedSpeed = m_phaseSpeedSpring.update(targetSpeed, dtAudio);
    if (smoothedSpeed > maxSpeed) smoothedSpeed = maxSpeed;
    if (smoothedSpeed < minSpeed) smoothedSpeed = minSpeed;

    m_phase += (0.40f + 1.80f * userSpeed) * smoothedSpeed * dtVisual;
    if (m_phase > 100000.0f) {
        m_phase = fmodf(m_phase, 6.2831853f);
    }

    // Mood-controlled burst decay (no BUILD accumulation, no separate REST decay)
    const float burstDecayTau = 0.26f - 0.10f * userMood;
    m_burst *= expf(-dtAudio / burstDecayTau);

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    const uint8_t binStep = static_cast<uint8_t>(255.0f / 12.0f);
    // Derive integer root bin from circular angle (avoids wrap-around artefacts)
    const uint8_t rootBin = static_cast<uint8_t>(
        roundf(m_keyRootAngle * (12.0f / effects::chroma::TWO_PI_F))) % 12;
    const uint8_t thirdBin = static_cast<uint8_t>((rootBin + (m_keyMinor ? 3 : 4)) % 12);
    const uint8_t fifthBin = static_cast<uint8_t>((rootBin + 7) % 12);

    const uint8_t hueRoot = static_cast<uint8_t>(ctx.gHue + rootBin * binStep);
    const uint8_t hueThird = static_cast<uint8_t>(ctx.gHue + thirdBin * binStep);
    const uint8_t hueFifth = static_cast<uint8_t>(ctx.gHue + fifthBin * binStep);

    const float freqBase = 0.18f + 0.30f * env;
    const float falloff = (3.6f - 1.2f * userMood) - (1.6f - 0.4f * userMood) * env;
    const float pulseRate = (0.6f + 0.6f * userSpeed) + (1.8f + 0.8f * userSpeed) * env;
    const uint8_t motionIdx =
        static_cast<uint8_t>((static_cast<uint32_t>(m_phase * 40.58f)) & 0xFFu);

    const int maxLeds = (ctx.ledCount < STRIP_LENGTH) ? static_cast<int>(ctx.ledCount)
                                                       : static_cast<int>(STRIP_LENGTH);

    for (int i = 0; i < maxLeds; ++i) {
        const float distFromCentre = static_cast<float>(centerPairDistance(static_cast<uint16_t>(i)));
        const float normalisedDist = distFromCentre / static_cast<float>(HALF_LENGTH);
        const float ray1 = sinf(distFromCentre * freqBase - m_phase);
        const float ray2 = 0.35f * env * sinf(distFromCentre * (freqBase * 2.0f) - (m_phase * 1.3f));

        float field = 0.5f + 0.5f * (ray1 + ray2);
        field *= expf(-normalisedDist * falloff);
        field *= 0.35f + 0.65f * (0.5f + 0.5f * sinf(m_phase * pulseRate));

        if (env > 0.02f) {
            field += m_burst * env * expf(-normalisedDist * (falloff + 0.6f));
        }
        field = clamp01(field);

        // Controllable contrast curve (replaces harsh squaring)
        const float gamma = 2.2f - 0.8f * userMood;
        field = powf(field, gamma);

        // Kick-locked amplitude modulation
        field *= (0.70f + 0.30f * m_kickEnv);

        float brightF = field;
        if (m_storyPhase == StoryPhase::REST) {
            brightF *= 0.20f;
        } else if (m_storyPhase == StoryPhase::BUILD) {
            brightF *= (0.35f + 0.65f * env);
        }

        // Soft limiter (prevents flash/white-out)
        brightF = brightF / (brightF + 0.15f) * 1.15f;
        brightF = clamp01(brightF);

        const uint8_t brightness =
            clampU8(static_cast<int>(roundf(brightF * intensityNorm * 255.0f)));
        const int baseIdx = static_cast<int>(distFromCentre * 2.0f) + static_cast<int>(motionIdx);
        const uint8_t paletteIndex = clampU8(baseIdx);

        const float t = clamp01(normalisedDist);
        float wRoot = clamp01(1.15f - 1.65f * t);
        float wFifth = clamp01(0.35f + 0.95f * t);
        float wThird = env * clamp01(1.0f - fabsf(t - 0.35f) * 3.0f);
        const float weightSum = wRoot + wThird + wFifth;
        if (weightSum > 0.0001f) {
            wRoot /= weightSum;
            wThird /= weightSum;
            wFifth /= weightSum;
        }

        const uint8_t bRoot = clampU8(static_cast<int>(roundf(brightness * wRoot)));
        const uint8_t bThird = clampU8(static_cast<int>(roundf(brightness * wThird)));
        const uint8_t bFifth = clampU8(static_cast<int>(roundf(brightness * wFifth)));

        CRGB c = CRGB::Black;
        if (bRoot) c += ctx.palette.getColor(static_cast<uint8_t>(hueRoot + paletteIndex), bRoot);
        if (bThird) c += ctx.palette.getColor(static_cast<uint8_t>(hueThird + paletteIndex), bThird);
        if (bFifth) c += ctx.palette.getColor(static_cast<uint8_t>(hueFifth + paletteIndex), bFifth);
        if (m_burst > 0.20f && env > 0.25f) {
            const uint8_t accentB = clampU8(static_cast<int>(roundf(brightness * m_burst * 0.55f)));
            c += ctx.palette.getColor(static_cast<uint8_t>(hueRoot + 128 + paletteIndex), accentB);
        }

        ctx.leds[i] += c;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            const uint8_t harmonyShift = 85;
            CRGB c2 = CRGB::Black;
            if (bRoot) c2 += ctx.palette.getColor(static_cast<uint8_t>(hueRoot + harmonyShift + paletteIndex), bRoot);
            if (bThird) c2 += ctx.palette.getColor(static_cast<uint8_t>(hueThird + harmonyShift + paletteIndex), bThird);
            if (bFifth) c2 += ctx.palette.getColor(static_cast<uint8_t>(hueFifth + harmonyShift + paletteIndex), bFifth);
            if (m_burst > 0.20f && env > 0.25f) {
                const uint8_t accentB = clampU8(static_cast<int>(roundf(brightness * m_burst * 0.55f)));
                c2 += ctx.palette.getColor(static_cast<uint8_t>(hueRoot + harmonyShift + 128 + paletteIndex), accentB);
            }
            ctx.leds[i + STRIP_LENGTH] += c2;
        }
    }
}

void LGPStarBurstNarrativeEffect::cleanup() {
    // No resources to free.
}

const plugins::EffectMetadata& LGPStarBurstNarrativeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Star Burst (Narrative)",
        "Legacy starburst core with phrase-gated harmonic commits",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
