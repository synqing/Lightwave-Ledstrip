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

LGPTimeReversalMirrorEffect_Mod1::LGPTimeReversalMirrorEffect_Mod1() = default;

void LGPTimeReversalMirrorEffect_Mod1::seedField(int z) {
    if (!m_ps) return;
    for (uint16_t i = 0; i < kFieldSize; i++) {
        float distNorm = static_cast<float>(i) / static_cast<float>(kFieldSize - 1);
        float bump = expf(-distNorm * distNorm * 18.0f) * 0.3f;
        m_ps->u_curr[z][i] = 0.5f + bump;
        m_ps->u_prev[z][i] = 0.5f;
        m_ps->u_next[z][i] = 0.5f;
    }
    m_normMin[z] = 0.45f;
    m_normMax[z] = 0.55f;
}

void LGPTimeReversalMirrorEffect_Mod1::beginForwardPhase(int z, bool reseedField) {
    m_isReverse[z] = false;
    m_phaseTimer[z] = 0.0f;
    m_frameInPhase[z] = 0;
    m_historyWrite[z] = 0;
    m_historyCount[z] = 0;
    m_reverseCursor[z] = 0.0f;
    m_frameSinceImpulse[z] = 0;
    m_framesSinceBeatImpulse[z] = 0;

    if (reseedField) {
        seedField(z);
        m_introPhase[z] = 0.0f;
        return;
    }

    // Preserve continuity between cycles while gently re-centering the field.
    for (uint16_t i = 0; i < kFieldSize; i++) {
        float carry = clampf(m_ps->u_curr[z][i], 0.0f, 1.0f);
        float centred = 0.92f * carry + 0.08f * 0.5f;
        m_ps->u_curr[z][i] = centred;
        m_ps->u_prev[z][i] = centred;
        m_ps->u_next[z][i] = centred;
    }
    for (uint16_t k = 0; k < 12; k++) {
        float g = expf(-(float)(k * k) * 0.22f) * 0.035f;
        m_ps->u_curr[z][k] = clampf(m_ps->u_curr[z][k] + g, 0.0f, 1.0f);
    }
}

void LGPTimeReversalMirrorEffect_Mod1::beginReversePhase(int z) {
    m_isReverse[z] = true;
    m_phaseTimer[z] = 0.0f;
    m_frameInPhase[z] = 0;
    m_reverseCursor[z] = (m_historyCount[z] > 0) ? static_cast<float>(m_historyCount[z] - 1) : 0.0f;
}

uint16_t LGPTimeReversalMirrorEffect_Mod1::historySlotFromChrono(int z, uint16_t chronoIndex) const {
    // chronoIndex is oldest->newest over [0 .. m_historyCount-1].
    if (m_historyCount[z] < kHistoryDepth) {
        return chronoIndex;
    }
    uint16_t oldest = m_historyWrite[z];  // Next write slot is oldest frame in a full ring.
    return static_cast<uint16_t>((oldest + chronoIndex) % kHistoryDepth);
}

bool LGPTimeReversalMirrorEffect_Mod1::init(plugins::EffectContext& ctx) {
    (void)ctx;

#ifndef NATIVE_BUILD
    const bool wasFirstAlloc = (m_ps == nullptr);
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPTimeReversalMirror_Mod1: PSRAM alloc failed (%u bytes)",
                    (unsigned)sizeof(PsramData));
            return false;
        }
    }
    // Only zero the live field arrays (~960 B) on re-init. The 320 KB history
    // buffer is gated by m_historyCount=0 below — stale data in it is never
    // read. Zeroing the full struct costs ~64 ms over PSRAM SPI and was the
    // dominant stall driver under rapid effect cycling.
    if (wasFirstAlloc) {
        memset(m_ps, 0, sizeof(PsramData));  // first-time init: fully zero
    } else {
        memset(m_ps->u_prev, 0, sizeof(m_ps->u_prev));
        memset(m_ps->u_curr, 0, sizeof(m_ps->u_curr));
        memset(m_ps->u_next, 0, sizeof(m_ps->u_next));
    }
#else
    m_ps = nullptr;
#endif

    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_phaseTimer[zi] = 0.0f;
        m_isReverse[zi] = false;
        m_frameInPhase[zi] = 0;
        m_historyWrite[zi] = 0;
        m_historyCount[zi] = 0;
        m_reverseCursor[zi] = 0.0f;
        m_frameSinceImpulse[zi] = 0;
        m_framesSinceBeatImpulse[zi] = 0;
        m_storyTime[zi] = 0.0f;
        m_introPhase[zi] = 0.0f;
        m_normMin[zi] = 0.45f;
        m_normMax[zi] = 0.55f;
        m_fallbackPhase[zi] = 0.0f;
        beginForwardPhase(zi, true);
    }

#if FEATURE_AUDIO_SYNC
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        for (uint8_t i = 0; i < 12; i++) {
            m_chromaFollowers[zi][i].reset(0.0f);
            m_chromaSmoothed[zi][i] = 0.0f;
            m_chromaTargets[zi][i] = 0.0f;
        }
        m_chromaAngle[zi] = 0.0f;
        m_rmsFollower[zi].reset(0.0f);
        m_targetRms[zi] = 0.0f;
        m_lastHopSeq[zi] = 0;
    }
#endif

    return true;
}

void LGPTimeReversalMirrorEffect_Mod1::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

    float* u_prev = m_ps->u_prev[z];
    float* u_curr = m_ps->u_curr[z];
    float* u_next = m_ps->u_next[z];
    float (*history)[kFieldSize] = m_ps->history[z];

    float& phaseTimer = m_phaseTimer[z];
    bool& isReverse = m_isReverse[z];
    uint16_t& frameInPhase = m_frameInPhase[z];
    uint16_t& historyWrite = m_historyWrite[z];
    uint16_t& historyCount = m_historyCount[z];
    float& reverseCursor = m_reverseCursor[z];
    uint16_t& frameSinceImpulse = m_frameSinceImpulse[z];
    uint16_t& framesSinceBeatImpulse = m_framesSinceBeatImpulse[z];
    float& storyTime = m_storyTime[z];
    float& introPhase = m_introPhase[z];
    float& normMin = m_normMin[z];
    float& normMax = m_normMax[z];
    float& fallbackPhase = m_fallbackPhase[z];

    float rawDt = ctx.getSafeRawDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float moodNorm = ctx.getMoodNormalized();
    storyTime += rawDt;

    // Slow modulators (irrationally related rates) for long-form non-repeating evolution.
    float storyA = 0.5f + 0.5f * sinf(storyTime * 0.071f);
    float storyB = 0.5f + 0.5f * sinf(storyTime * 0.113f + 1.7f);

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
    float* chromaSmoothed = m_chromaSmoothed[z];
    float* chromaTargets = m_chromaTargets[z];
    enhancement::AsymmetricFollower* chromaFollowers = m_chromaFollowers[z];
    float& chromaAngle = m_chromaAngle[z];
    enhancement::AsymmetricFollower& rmsFollower = m_rmsFollower[z];
    float& targetRms = m_targetRms[z];
    uint32_t& lastHopSeq = m_lastHopSeq[z];

    if (ctx.audio.available) {
        bool newHop = (ctx.audio.hopSequence() != lastHopSeq);
        if (newHop) {
            lastHopSeq = ctx.audio.hopSequence();
            targetRms = ctx.audio.rms();
            for (uint8_t i = 0; i < 12; i++) {
                chromaTargets[i] = ctx.audio.getHeavyChroma(i);
            }
        }

        float smoothedRms = rmsFollower.updateWithMood(targetRms, rawDt, moodNorm);
        for (uint8_t i = 0; i < 12; i++) {
            chromaSmoothed[i] = chromaFollowers[i].updateWithMood(
                chromaTargets[i], rawDt, moodNorm);
        }

        chromaHue = effects::chroma::circularChromaHueSmoothed(
            chromaSmoothed, chromaAngle, rawDt, 0.20f);

        // Narrower range than base effect to keep wave launches coherent.
        impulseStrength = 0.42f + 0.46f * clampf(smoothedRms * 1.8f, 0.0f, 1.0f);
        beatTriggered = ctx.audio.isOnBeat();
    } else {
        fallbackPhase += speedNorm * 0.35f * rawDt;
        if (fallbackPhase > 6.2831853f) fallbackPhase -= 6.2831853f;
        chromaHue = static_cast<uint8_t>(fallbackPhase * (255.0f / 6.2831853f));
    }
#else
    fallbackPhase += speedNorm * 0.35f * rawDt;
    if (fallbackPhase > 6.2831853f) fallbackPhase -= 6.2831853f;
    chromaHue = static_cast<uint8_t>(fallbackPhase * (255.0f / 6.2831853f));
#endif

    phaseTimer += rawDt;
    frameInPhase++;

    if (!isReverse) {
        if (phaseTimer >= forwardDur && historyCount > 8) {
            beginReversePhase(z);
        } else {
            // Fluid opening: continuous centre drive envelope to avoid staccato starts.
            float introProgress = clampf(phaseTimer / m_introSec, 0.0f, 1.0f);
            float introEnv = 1.0f - smooth01(introProgress);
            introPhase += rawDt * 6.2831853f * (0.95f + 0.45f * storyB);
            if (introPhase > 6.2831853f) introPhase -= 6.2831853f;
            float introCarrier = 0.5f + 0.5f * sinf(introPhase);
            float introGain = introEnv * (m_introDrive + 0.03f * storyA) * (0.65f + 0.35f * introCarrier);
            if (introGain > 0.0001f) {
                for (uint16_t k = 0; k < 16; k++) {
                    float g = expf(-(float)(k * k) * 0.18f) * introGain;
                    u_curr[k] = clampf(u_curr[k] + g, 0.0f, 1.0f);
                }
            }

            frameSinceImpulse++;
            framesSinceBeatImpulse++;
            bool doImpulse = (frameSinceImpulse >= impulseEveryLocal);

#if FEATURE_AUDIO_SYNC
            uint16_t beatCooldown = (introEnv > 0.05f)
                                  ? static_cast<uint16_t>(kBeatImpulseCooldownFrames * 2)
                                  : kBeatImpulseCooldownFrames;
            if (beatTriggered && framesSinceBeatImpulse >= beatCooldown) {
                doImpulse = true;
            }
#endif

            if (doImpulse) {
                frameSinceImpulse = 0;
                framesSinceBeatImpulse = 0;
                float pulseStrength = impulseStrength * (0.68f + 0.32f * smooth01(introProgress));
                for (uint16_t k = 0; k < 10; k++) {
                    float g = expf(-(float)(k * k) * 0.35f) * pulseStrength * 0.19f;
                    u_curr[k] += g;
                    u_curr[k] = clampf(u_curr[k], 0.0f, 1.0f);
                }
            }

            float moodDamping = baseDamping * (0.92f + 0.28f * moodNorm);

            // 1D damped wave. Centre uses mirrored neighbour; edge has soft absorption.
            for (uint16_t i = 0; i < kFieldSize; i++) {
                float left;
                float right;
                if (i == 0) {
                    left = u_curr[1];
                    right = u_curr[1];
                } else if (i < kFieldSize - 1) {
                    left = u_curr[i - 1];
                    right = u_curr[i + 1];
                } else {
                    left = u_curr[i - 1];
                    right = u_curr[i];
                }

                float laplacian = left - 2.0f * u_curr[i] + right;

                float edgeNorm = static_cast<float>(i) / static_cast<float>(kFieldSize - 1);
                float edgeFactor = clampf((edgeNorm - 0.75f) / 0.25f, 0.0f, 1.0f);
                float localDamping = moodDamping + edgeFactor * m_edgeAbsorb;

                u_next[i] = 2.0f * u_curr[i]
                                - u_prev[i]
                                + cSqLocal * laplacian
                                - localDamping * u_curr[i];

                u_next[i] = clampf(u_next[i], -0.35f, 1.35f);
            }

            memcpy(u_prev, u_curr, sizeof(float) * kFieldSize);
            memcpy(u_curr, u_next, sizeof(float) * kFieldSize);

            // True ring-buffer write.
            memcpy(history[historyWrite], u_curr, sizeof(float) * kFieldSize);
            historyWrite = static_cast<uint16_t>((historyWrite + 1) % kHistoryDepth);
            if (historyCount < kHistoryDepth) {
                historyCount++;
            }
        }
    } else {
        if (historyCount < 2) {
            beginForwardPhase(z, true);
        } else {
            float maxCursor = static_cast<float>(historyCount - 1);
            float cursor = clampf(reverseCursor, 0.0f, maxCursor);

            uint16_t c0 = static_cast<uint16_t>(floorf(cursor));
            uint16_t c1 = (c0 + 1 < historyCount) ? static_cast<uint16_t>(c0 + 1) : c0;
            float t = cursor - static_cast<float>(c0);

            uint16_t slot0 = historySlotFromChrono(z, c0);
            uint16_t slot1 = historySlotFromChrono(z, c1);
            const float* snap0 = history[slot0];
            const float* snap1 = history[slot1];

            for (uint16_t i = 0; i < kFieldSize; i++) {
                float v = snap0[i] + (snap1[i] - snap0[i]) * t;
                u_curr[i] = 1.0f - v;  // Phase flip around 0.5
            }

            float reverseRate = maxCursor / fmaxf(reverseDur, 0.1f);
            reverseCursor -= reverseRate * rawDt;

            // Exit immediately after reaching frame 0 to avoid a held "stuck" tail.
            if (phaseTimer >= reverseDur || reverseCursor <= 0.0f) {
                beginForwardPhase(z, false);
            }
        }
    }

    float fieldMin = u_curr[0];
    float fieldMax = u_curr[0];
    for (uint16_t i = 1; i < kFieldSize; i++) {
        if (u_curr[i] < fieldMin) fieldMin = u_curr[i];
        if (u_curr[i] > fieldMax) fieldMax = u_curr[i];
    }

    float followAlpha = clampf(rawDt * m_normaliseFollowHz, 0.02f, 1.0f);
    normMin += (fieldMin - normMin) * followAlpha;
    normMax += (fieldMax - normMax) * followAlpha;

    float range = normMax - normMin;
    if (range < 0.05f) {
        float mid = 0.5f * (normMin + normMax);
        normMin = mid - 0.025f;
        normMax = mid + 0.025f;
        range = 0.05f;
    }

    uint8_t reverseHueShift = isReverse ? 16 : 0;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        uint16_t fi = (dist < kFieldSize) ? dist : static_cast<uint16_t>(kFieldSize - 1);

        float fieldVal = (u_curr[fi] - normMin) / range;
        fieldVal = clampf(fieldVal, 0.0f, 1.0f);
        float sculpted = powf(fieldVal, m_peakGamma);  // Preserve sharp "fang" peaks.
        uint8_t brightness = static_cast<uint8_t>(sculpted * static_cast<float>(ctx.brightness));

        uint8_t spatialHue = static_cast<uint8_t>(static_cast<float>(dist) * 0.45f);
        if (spatialHue > 36) spatialHue = 36;
        uint8_t hue = static_cast<uint8_t>(ctx.gHue + chromaHue + spatialHue + reverseHueShift);

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);

        // Strip 2: reduced offset for stronger coherence, still visually separated.
        uint16_t fi2 = (fi + 8 < kFieldSize) ? static_cast<uint16_t>(fi + 8) : static_cast<uint16_t>(kFieldSize - 1);
        float fieldVal2 = (u_curr[fi2] - normMin) / range;
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
