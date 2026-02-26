/**
 * @file LGPTimeReversalMirrorEffect_Mod1.cpp
 * @brief LGP Time-Reversal Mirror Mod1 implementation
 *
 * Mod1 refactor keeps the same visual concept but improves continuity:
 *   - Reverse phase reads history with interpolation (no step/jump artefacts)
 *   - Reverse phase exits when cursor reaches frame 0 (no held freeze)
 *   - History is a true ring with larger depth for better narrative continuity
 *   - Edge damping is spatially weighted to reduce harsh outer-edge flashing
 *   - Brightness normalisation is temporally smoothed to reduce pumping
 */

#include "LGPTimeReversalMirrorEffect_Mod1.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../utils/Log.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
const plugins::EffectParameter kParameters[] = {
    {"csq", "Wave Propagation", 0.01f, 0.40f, 0.14f,
     plugins::EffectParameterType::FLOAT, 0.005f, "wave", "", false},
    {"damping", "Damping", 0.005f, 0.20f, 0.035f,
     plugins::EffectParameterType::FLOAT, 0.002f, "wave", "", false},
    {"edge_absorb", "Edge Absorb", 0.00f, 0.30f, 0.09f,
     plugins::EffectParameterType::FLOAT, 0.005f, "wave", "", false},
    {"impulse_every", "Impulse Every", 16.0f, 240.0f, 96.0f,
     plugins::EffectParameterType::INT, 1.0f, "timing", "frames", false},
    {"forward_sec", "Forward Seconds", 1.0f, 30.0f, 6.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "s", false},
    {"reverse_sec", "Reverse Seconds", 0.5f, 30.0f, 3.75f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "s", false},
    {"intro_sec", "Intro Seconds", 0.1f, 8.0f, 1.6f,
     plugins::EffectParameterType::FLOAT, 0.05f, "intro", "s", false},
    {"intro_drive", "Intro Drive", 0.0f, 0.30f, 0.07f,
     plugins::EffectParameterType::FLOAT, 0.005f, "intro", "", false},
    {"normalise_follow_hz", "Normalise Follow", 0.5f, 20.0f, 6.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "blend", "Hz", false},
    {"peak_gamma", "Peak Gamma", 0.5f, 3.0f, 1.35f,
     plugins::EffectParameterType::FLOAT, 0.05f, "ridge", "", false},
};
}

static inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

static inline float smooth01(float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

LGPTimeReversalMirrorEffect_Mod1::LGPTimeReversalMirrorEffect_Mod1()
    : m_ps(nullptr)
    , m_phaseTimer(0.0f)
    , m_isReverse(false)
    , m_frameInPhase(0)
    , m_historyWrite(0)
    , m_historyCount(0)
    , m_reverseCursor(0.0f)
    , m_frameSinceImpulse(0)
    , m_framesSinceBeatImpulse(0)
    , m_storyTime(0.0f)
    , m_introPhase(0.0f)
    , m_normMin(0.45f)
    , m_normMax(0.55f)
    , m_fallbackPhase(0.0f)
{
}

void LGPTimeReversalMirrorEffect_Mod1::seedField() {
    if (!m_ps) return;
    for (uint16_t i = 0; i < kFieldSize; i++) {
        float distNorm = static_cast<float>(i) / static_cast<float>(kFieldSize - 1);
        float bump = expf(-distNorm * distNorm * 18.0f) * 0.3f;
        m_ps->u_curr[i] = 0.5f + bump;
        m_ps->u_prev[i] = 0.5f;
        m_ps->u_next[i] = 0.5f;
    }
    m_normMin = 0.45f;
    m_normMax = 0.55f;
}

void LGPTimeReversalMirrorEffect_Mod1::beginForwardPhase(bool reseedField) {
    m_isReverse = false;
    m_phaseTimer = 0.0f;
    m_frameInPhase = 0;
    m_historyWrite = 0;
    m_historyCount = 0;
    m_reverseCursor = 0.0f;
    m_frameSinceImpulse = 0;
    m_framesSinceBeatImpulse = 0;

    if (reseedField) {
        seedField();
        m_introPhase = 0.0f;
        return;
    }

    // Preserve continuity between cycles while gently re-centering the field.
    for (uint16_t i = 0; i < kFieldSize; i++) {
        float carry = clampf(m_ps->u_curr[i], 0.0f, 1.0f);
        float centred = 0.92f * carry + 0.08f * 0.5f;
        m_ps->u_curr[i] = centred;
        m_ps->u_prev[i] = centred;
        m_ps->u_next[i] = centred;
    }
    for (uint16_t k = 0; k < 12; k++) {
        float g = expf(-(float)(k * k) * 0.22f) * 0.035f;
        m_ps->u_curr[k] = clampf(m_ps->u_curr[k] + g, 0.0f, 1.0f);
    }
}

void LGPTimeReversalMirrorEffect_Mod1::beginReversePhase() {
    m_isReverse = true;
    m_phaseTimer = 0.0f;
    m_frameInPhase = 0;
    m_reverseCursor = (m_historyCount > 0) ? static_cast<float>(m_historyCount - 1) : 0.0f;
}

uint16_t LGPTimeReversalMirrorEffect_Mod1::historySlotFromChrono(uint16_t chronoIndex) const {
    // chronoIndex is oldest->newest over [0 .. m_historyCount-1].
    if (m_historyCount < kHistoryDepth) {
        return chronoIndex;
    }
    uint16_t oldest = m_historyWrite;  // Next write slot is oldest frame in a full ring.
    return static_cast<uint16_t>((oldest + chronoIndex) % kHistoryDepth);
}

bool LGPTimeReversalMirrorEffect_Mod1::init(plugins::EffectContext& ctx) {
    (void)ctx;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPTimeReversalMirror_Mod1: PSRAM alloc failed (%u bytes)",
                    (unsigned)sizeof(PsramData));
            return false;
        }
    }
    memset(m_ps, 0, sizeof(PsramData));
#else
    m_ps = nullptr;
#endif

    m_phaseTimer = 0.0f;
    m_isReverse = false;
    m_frameInPhase = 0;
    m_historyWrite = 0;
    m_historyCount = 0;
    m_reverseCursor = 0.0f;
    m_frameSinceImpulse = 0;
    m_framesSinceBeatImpulse = 0;
    m_storyTime = 0.0f;
    m_introPhase = 0.0f;
    m_normMin = 0.45f;
    m_normMax = 0.55f;
    m_fallbackPhase = 0.0f;

    beginForwardPhase(true);

#if FEATURE_AUDIO_SYNC
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaSmoothed[i] = 0.0f;
        m_chromaTargets[i] = 0.0f;
    }
    m_chromaAngle = 0.0f;
    m_rmsFollower.reset(0.0f);
    m_targetRms = 0.0f;
    m_lastHopSeq = 0;
#endif

    return true;
}

void LGPTimeReversalMirrorEffect_Mod1::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;

    float rawDt = ctx.getSafeRawDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float moodNorm = ctx.getMoodNormalized();
    m_storyTime += rawDt;

    // Slow modulators (irrationally related rates) for long-form non-repeating evolution.
    float storyA = 0.5f + 0.5f * sinf(m_storyTime * 0.071f);
    float storyB = 0.5f + 0.5f * sinf(m_storyTime * 0.113f + 1.7f);

    float forwardDur = (m_forwardSec * (0.88f + 0.24f * storyB)) / fmaxf(speedNorm, 0.2f);
    float reverseDur = (m_reverseSec * (0.90f + 0.20f * (1.0f - storyA))) / fmaxf(speedNorm, 0.2f);
    float cSqLocal = m_csq * (0.92f + 0.22f * storyA);
    float baseDamping = m_damping * (0.90f + 0.26f * storyB);
    uint16_t impulseEveryLocal = static_cast<uint16_t>(
        static_cast<float>(m_impulseEvery) * (0.82f + 0.36f * (1.0f - storyA)));
    if (impulseEveryLocal < 24) impulseEveryLocal = 24;

    float impulseStrength = 0.58f;
    uint8_t chromaHue = 0;

#if FEATURE_AUDIO_SYNC
    bool beatTriggered = false;
    if (ctx.audio.available) {
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            m_targetRms = ctx.audio.rms();
            for (uint8_t i = 0; i < 12; i++) {
                m_chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }
        }

        float smoothedRms = m_rmsFollower.updateWithMood(m_targetRms, rawDt, moodNorm);
        for (uint8_t i = 0; i < 12; i++) {
            m_chromaSmoothed[i] = m_chromaFollowers[i].updateWithMood(
                m_chromaTargets[i], rawDt, moodNorm);
        }

        chromaHue = effects::chroma::circularChromaHueSmoothed(
            m_chromaSmoothed, m_chromaAngle, rawDt, 0.20f);

        // Narrower range than base effect to keep wave launches coherent.
        impulseStrength = 0.42f + 0.46f * clampf(smoothedRms * 1.8f, 0.0f, 1.0f);
        beatTriggered = ctx.audio.isOnBeat();
    } else {
        m_fallbackPhase += speedNorm * 0.35f * rawDt;
        if (m_fallbackPhase > 6.2831853f) m_fallbackPhase -= 6.2831853f;
        chromaHue = static_cast<uint8_t>(m_fallbackPhase * (255.0f / 6.2831853f));
    }
#else
    m_fallbackPhase += speedNorm * 0.35f * rawDt;
    if (m_fallbackPhase > 6.2831853f) m_fallbackPhase -= 6.2831853f;
    chromaHue = static_cast<uint8_t>(m_fallbackPhase * (255.0f / 6.2831853f));
#endif

    m_phaseTimer += rawDt;
    m_frameInPhase++;

    if (!m_isReverse) {
        if (m_phaseTimer >= forwardDur && m_historyCount > 8) {
            beginReversePhase();
        } else {
            // Fluid opening: continuous centre drive envelope to avoid staccato starts.
            float introProgress = clampf(m_phaseTimer / m_introSec, 0.0f, 1.0f);
            float introEnv = 1.0f - smooth01(introProgress);
            m_introPhase += rawDt * 6.2831853f * (0.95f + 0.45f * storyB);
            if (m_introPhase > 6.2831853f) m_introPhase -= 6.2831853f;
            float introCarrier = 0.5f + 0.5f * sinf(m_introPhase);
            float introGain = introEnv * (m_introDrive + 0.03f * storyA) * (0.65f + 0.35f * introCarrier);
            if (introGain > 0.0001f) {
                for (uint16_t k = 0; k < 16; k++) {
                    float g = expf(-(float)(k * k) * 0.18f) * introGain;
                    m_ps->u_curr[k] = clampf(m_ps->u_curr[k] + g, 0.0f, 1.0f);
                }
            }

            m_frameSinceImpulse++;
            m_framesSinceBeatImpulse++;
            bool doImpulse = (m_frameSinceImpulse >= impulseEveryLocal);

#if FEATURE_AUDIO_SYNC
            uint16_t beatCooldown = (introEnv > 0.05f)
                                  ? static_cast<uint16_t>(kBeatImpulseCooldownFrames * 2)
                                  : kBeatImpulseCooldownFrames;
            if (beatTriggered && m_framesSinceBeatImpulse >= beatCooldown) {
                doImpulse = true;
            }
#endif

            if (doImpulse) {
                m_frameSinceImpulse = 0;
                m_framesSinceBeatImpulse = 0;
                float pulseStrength = impulseStrength * (0.68f + 0.32f * smooth01(introProgress));
                for (uint16_t k = 0; k < 10; k++) {
                    float g = expf(-(float)(k * k) * 0.35f) * pulseStrength * 0.19f;
                    m_ps->u_curr[k] += g;
                    m_ps->u_curr[k] = clampf(m_ps->u_curr[k], 0.0f, 1.0f);
                }
            }

            float moodDamping = baseDamping * (0.92f + 0.28f * moodNorm);

            // 1D damped wave. Centre uses mirrored neighbour; edge has soft absorption.
            for (uint16_t i = 0; i < kFieldSize; i++) {
                float left;
                float right;
                if (i == 0) {
                    left = m_ps->u_curr[1];
                    right = m_ps->u_curr[1];
                } else if (i < kFieldSize - 1) {
                    left = m_ps->u_curr[i - 1];
                    right = m_ps->u_curr[i + 1];
                } else {
                    left = m_ps->u_curr[i - 1];
                    right = m_ps->u_curr[i];
                }

                float laplacian = left - 2.0f * m_ps->u_curr[i] + right;

                float edgeNorm = static_cast<float>(i) / static_cast<float>(kFieldSize - 1);
                float edgeFactor = clampf((edgeNorm - 0.75f) / 0.25f, 0.0f, 1.0f);
                float localDamping = moodDamping + edgeFactor * m_edgeAbsorb;

                m_ps->u_next[i] = 2.0f * m_ps->u_curr[i]
                                - m_ps->u_prev[i]
                                + cSqLocal * laplacian
                                - localDamping * m_ps->u_curr[i];

                m_ps->u_next[i] = clampf(m_ps->u_next[i], -0.35f, 1.35f);
            }

            memcpy(m_ps->u_prev, m_ps->u_curr, sizeof(float) * kFieldSize);
            memcpy(m_ps->u_curr, m_ps->u_next, sizeof(float) * kFieldSize);

            // True ring-buffer write.
            memcpy(m_ps->history[m_historyWrite], m_ps->u_curr, sizeof(float) * kFieldSize);
            m_historyWrite = static_cast<uint16_t>((m_historyWrite + 1) % kHistoryDepth);
            if (m_historyCount < kHistoryDepth) {
                m_historyCount++;
            }
        }
    } else {
        if (m_historyCount < 2) {
            beginForwardPhase(true);
        } else {
            float maxCursor = static_cast<float>(m_historyCount - 1);
            float cursor = clampf(m_reverseCursor, 0.0f, maxCursor);

            uint16_t c0 = static_cast<uint16_t>(floorf(cursor));
            uint16_t c1 = (c0 + 1 < m_historyCount) ? static_cast<uint16_t>(c0 + 1) : c0;
            float t = cursor - static_cast<float>(c0);

            uint16_t slot0 = historySlotFromChrono(c0);
            uint16_t slot1 = historySlotFromChrono(c1);
            const float* snap0 = m_ps->history[slot0];
            const float* snap1 = m_ps->history[slot1];

            for (uint16_t i = 0; i < kFieldSize; i++) {
                float v = snap0[i] + (snap1[i] - snap0[i]) * t;
                m_ps->u_curr[i] = 1.0f - v;  // Phase flip around 0.5
            }

            float reverseRate = maxCursor / fmaxf(reverseDur, 0.1f);
            m_reverseCursor -= reverseRate * rawDt;

            // Exit immediately after reaching frame 0 to avoid a held "stuck" tail.
            if (m_phaseTimer >= reverseDur || m_reverseCursor <= 0.0f) {
                beginForwardPhase(false);
            }
        }
    }

    float fieldMin = m_ps->u_curr[0];
    float fieldMax = m_ps->u_curr[0];
    for (uint16_t i = 1; i < kFieldSize; i++) {
        if (m_ps->u_curr[i] < fieldMin) fieldMin = m_ps->u_curr[i];
        if (m_ps->u_curr[i] > fieldMax) fieldMax = m_ps->u_curr[i];
    }

    float followAlpha = clampf(rawDt * m_normaliseFollowHz, 0.02f, 1.0f);
    m_normMin += (fieldMin - m_normMin) * followAlpha;
    m_normMax += (fieldMax - m_normMax) * followAlpha;

    float range = m_normMax - m_normMin;
    if (range < 0.05f) {
        float mid = 0.5f * (m_normMin + m_normMax);
        m_normMin = mid - 0.025f;
        m_normMax = mid + 0.025f;
        range = 0.05f;
    }

    uint8_t reverseHueShift = m_isReverse ? 16 : 0;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        uint16_t fi = (dist < kFieldSize) ? dist : static_cast<uint16_t>(kFieldSize - 1);

        float fieldVal = (m_ps->u_curr[fi] - m_normMin) / range;
        fieldVal = clampf(fieldVal, 0.0f, 1.0f);
        float sculpted = powf(fieldVal, m_peakGamma);  // Preserve sharp "fang" peaks.
        uint8_t brightness = static_cast<uint8_t>(sculpted * static_cast<float>(ctx.brightness));

        uint8_t spatialHue = static_cast<uint8_t>(static_cast<float>(dist) * 0.45f);
        if (spatialHue > 36) spatialHue = 36;
        uint8_t hue = static_cast<uint8_t>(ctx.gHue + chromaHue + spatialHue + reverseHueShift);

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);

        // Strip 2: reduced offset for stronger coherence, still visually separated.
        uint16_t fi2 = (fi + 8 < kFieldSize) ? static_cast<uint16_t>(fi + 8) : static_cast<uint16_t>(kFieldSize - 1);
        float fieldVal2 = (m_ps->u_curr[fi2] - m_normMin) / range;
        fieldVal2 = clampf(fieldVal2, 0.0f, 1.0f);
        float sculpted2 = powf(fieldVal2, m_peakGamma);
        uint8_t brightness2 = static_cast<uint8_t>(sculpted2 * static_cast<float>(ctx.brightness));
        uint8_t hue2 = static_cast<uint8_t>(hue + 24);

        uint16_t s2idx = static_cast<uint16_t>(i + STRIP_LENGTH);
        if (s2idx < ctx.ledCount) {
            ctx.leds[s2idx] = ctx.palette.getColor(hue2, brightness2);
        }
    }
}

void LGPTimeReversalMirrorEffect_Mod1::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPTimeReversalMirrorEffect_Mod1::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Time-Reversal Mirror Mod1",
        "Coherent reverse-interpolated damped wave with phase-flipped rewind",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPTimeReversalMirrorEffect_Mod1::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPTimeReversalMirrorEffect_Mod1::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPTimeReversalMirrorEffect_Mod1::setParameter(const char* name, float value) {
    if (!name) return false;
    if (std::strcmp(name, "csq") == 0) { m_csq = clampf(value, 0.01f, 0.40f); return true; }
    if (std::strcmp(name, "damping") == 0) { m_damping = clampf(value, 0.005f, 0.20f); return true; }
    if (std::strcmp(name, "edge_absorb") == 0) { m_edgeAbsorb = clampf(value, 0.00f, 0.30f); return true; }
    if (std::strcmp(name, "impulse_every") == 0) {
        m_impulseEvery = static_cast<uint16_t>(clampf(value, 16.0f, 240.0f) + 0.5f);
        return true;
    }
    if (std::strcmp(name, "forward_sec") == 0) { m_forwardSec = clampf(value, 1.0f, 30.0f); return true; }
    if (std::strcmp(name, "reverse_sec") == 0) { m_reverseSec = clampf(value, 0.5f, 30.0f); return true; }
    if (std::strcmp(name, "intro_sec") == 0) { m_introSec = clampf(value, 0.1f, 8.0f); return true; }
    if (std::strcmp(name, "intro_drive") == 0) { m_introDrive = clampf(value, 0.0f, 0.30f); return true; }
    if (std::strcmp(name, "normalise_follow_hz") == 0) {
        m_normaliseFollowHz = clampf(value, 0.5f, 20.0f);
        return true;
    }
    if (std::strcmp(name, "peak_gamma") == 0) { m_peakGamma = clampf(value, 0.5f, 3.0f); return true; }
    return false;
}

float LGPTimeReversalMirrorEffect_Mod1::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (std::strcmp(name, "csq") == 0) return m_csq;
    if (std::strcmp(name, "damping") == 0) return m_damping;
    if (std::strcmp(name, "edge_absorb") == 0) return m_edgeAbsorb;
    if (std::strcmp(name, "impulse_every") == 0) return static_cast<float>(m_impulseEvery);
    if (std::strcmp(name, "forward_sec") == 0) return m_forwardSec;
    if (std::strcmp(name, "reverse_sec") == 0) return m_reverseSec;
    if (std::strcmp(name, "intro_sec") == 0) return m_introSec;
    if (std::strcmp(name, "intro_drive") == 0) return m_introDrive;
    if (std::strcmp(name, "normalise_follow_hz") == 0) return m_normaliseFollowHz;
    if (std::strcmp(name, "peak_gamma") == 0) return m_peakGamma;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
