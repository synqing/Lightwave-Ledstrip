/**
 * @file LGPTimeReversalMirrorEffect_Mod3.cpp
 * @brief LGP Time-Reversal Mirror Mod3 implementation
 *
 * Mod3 keeps the robust reverse architecture from Mod1/Mod2 and adds
 * a dedicated ridge envelope so fang edges remain stable over organic flow.
 */

#include "LGPTimeReversalMirrorEffect_Mod3.h"
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
    {"beat_release_sec", "Beat Release", 0.05f, 2.0f, 0.42f,
     plugins::EffectParameterType::FLOAT, 0.01f, "audio", "s", false},
    {"normalise_follow_hz", "Normalise Follow", 0.5f, 20.0f, 6.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "blend", "Hz", false},
    {"peak_gamma", "Peak Gamma", 0.5f, 3.0f, 1.35f,
     plugins::EffectParameterType::FLOAT, 0.05f, "ridge", "", false},
    {"ridge_attack_hz", "Ridge Attack", 0.5f, 60.0f, 18.0f,
     plugins::EffectParameterType::FLOAT, 0.5f, "ridge", "Hz", false},
    {"ridge_release_hz", "Ridge Release", 0.5f, 30.0f, 4.5f,
     plugins::EffectParameterType::FLOAT, 0.2f, "ridge", "Hz", false},
    {"ridge_sensitivity", "Ridge Sensitivity", 0.2f, 10.0f, 3.6f,
     plugins::EffectParameterType::FLOAT, 0.1f, "ridge", "", false},
    {"ridge_blend", "Ridge Blend", 0.0f, 1.0f, 0.56f,
     plugins::EffectParameterType::FLOAT, 0.01f, "ridge", "", false},
    {"ridge_floor", "Ridge Floor", 0.0f, 0.5f, 0.05f,
     plugins::EffectParameterType::FLOAT, 0.005f, "ridge", "", false},
};
}

static inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

static inline float smooth01(float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static inline float sampleFieldLinear(const float* field, uint16_t size, float index) {
    if (size == 0) return 0.0f;
    if (index <= 0.0f) return field[0];
    float hi = static_cast<float>(size - 1);
    if (index >= hi) return field[size - 1];
    uint16_t i0 = static_cast<uint16_t>(index);
    uint16_t i1 = static_cast<uint16_t>(i0 + 1);
    float t = index - static_cast<float>(i0);
    return field[i0] + (field[i1] - field[i0]) * t;
}

LGPTimeReversalMirrorEffect_Mod3::LGPTimeReversalMirrorEffect_Mod3() = default;

void LGPTimeReversalMirrorEffect_Mod3::seedField(int z) {
    if (!m_ps) return;
    for (uint16_t i = 0; i < kFieldSize; i++) {
        float distNorm = static_cast<float>(i) / static_cast<float>(kFieldSize - 1);
        float bump = expf(-distNorm * distNorm * 18.0f) * 0.3f;
        m_ps->u_curr[z][i] = 0.5f + bump;
        m_ps->u_prev[z][i] = 0.5f;
        m_ps->u_next[z][i] = 0.5f;
        m_ridgeEnv[z][i] = 0.0f;
    }
    m_normMin[z] = 0.45f;
    m_normMax[z] = 0.55f;
}

void LGPTimeReversalMirrorEffect_Mod3::beginForwardPhase(int z, bool reseedField) {
    m_isReverse[z] = false;
    m_phaseTimer[z] = 0.0f;
    m_frameInPhase[z] = 0;
    m_historyWrite[z] = 0;
    m_historyCount[z] = 0;
    m_reverseCursor[z] = 0.0f;
    m_frameSinceImpulse[z] = 0;
    m_framesSinceBeatImpulse[z] = 0;
    m_beatEnv[z] *= 0.6f;
    for (uint16_t i = 0; i < kFieldSize; i++) {
        m_ridgeEnv[z][i] *= 0.75f;
    }

    if (reseedField) {
        seedField(z);
        m_introPhase[z] = 0.0f;
        m_beatEnv[z] = 0.0f;
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

void LGPTimeReversalMirrorEffect_Mod3::beginReversePhase(int z) {
    m_isReverse[z] = true;
    m_phaseTimer[z] = 0.0f;
    m_frameInPhase[z] = 0;
    m_reverseCursor[z] = (m_historyCount[z] > 0) ? static_cast<float>(m_historyCount[z] - 1) : 0.0f;
}

uint16_t LGPTimeReversalMirrorEffect_Mod3::historySlotFromChrono(int z, uint16_t chronoIndex) const {
    // chronoIndex is oldest->newest over [0 .. m_historyCount-1].
    if (m_historyCount[z] < kHistoryDepth) {
        return chronoIndex;
    }
    uint16_t oldest = m_historyWrite[z];  // Next write slot is oldest frame in a full ring.
    return static_cast<uint16_t>((oldest + chronoIndex) % kHistoryDepth);
}

bool LGPTimeReversalMirrorEffect_Mod3::init(plugins::EffectContext& ctx) {
    (void)ctx;

#ifndef NATIVE_BUILD
    const bool wasFirstAlloc = (m_ps == nullptr);
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPTimeReversalMirror_Mod3: PSRAM alloc failed (%u bytes)",
                    (unsigned)sizeof(PsramData));
            return false;
        }
    }
    // Zero only the active fields on re-init; history buffer gated by m_historyCount.
    if (wasFirstAlloc) {
        memset(m_ps, 0, sizeof(PsramData));
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
        m_beatEnv[zi] = 0.0f;
        for (uint16_t i = 0; i < kFieldSize; i++) {
            m_ridgeEnv[zi][i] = 0.0f;
        }
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

void LGPTimeReversalMirrorEffect_Mod3::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

    float* u_prev = m_ps->u_prev[z];
    float* u_curr = m_ps->u_curr[z];
    float* u_next = m_ps->u_next[z];
    float (*history)[kFieldSize] = m_ps->history[z];
    float* ridgeEnv = m_ridgeEnv[z];

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
    float& beatEnv = m_beatEnv[z];
    float& normMin = m_normMin[z];
    float& normMax = m_normMax[z];
    float& fallbackPhase = m_fallbackPhase[z];

    float rawDt = ctx.getSafeRawDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float moodNorm = ctx.getMoodNormalized();
    storyTime += rawDt;

    // Continuous multi-layer modulation: slow blends, no random cadence jumps.
    float layerA = 0.5f + 0.5f * sinf(storyTime * 0.071f);
    float layerB = 0.5f + 0.5f * sinf(storyTime * 0.113f + 1.7f);
    float layerC = 0.5f + 0.5f * sinf(storyTime * 0.167f + 2.9f);
    float fluidBlend = 0.52f * layerA + 0.30f * layerB + 0.18f * layerC;

    float forwardDur = (m_forwardSec * (0.92f + 0.20f * layerB)) / fmaxf(speedNorm, 0.2f);
    float reverseDur = (m_reverseSec * (0.90f + 0.18f * (1.0f - layerA))) / fmaxf(speedNorm, 0.2f);
    float cSqLocal = m_csq * (0.94f + 0.18f * fluidBlend);
    float baseDamping = m_damping * (0.88f + 0.24f * (1.0f - fluidBlend));

    float impulseStrength = 0.58f;
    uint8_t chromaHue = 0;
    beatEnv *= expf(-rawDt / m_beatReleaseSec);

#if FEATURE_AUDIO_SYNC
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
        if (ctx.audio.isOnBeat()) {
            beatEnv = 1.0f;
        }
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
            // Continuous centre drive with layered carriers for organic flow.
            float introProgress = clampf(phaseTimer / m_introSec, 0.0f, 1.0f);
            float introEnv = 1.0f - smooth01(introProgress);
            introPhase += rawDt * 6.2831853f * (0.90f + 0.40f * fluidBlend);
            if (introPhase > 6.2831853f) introPhase -= 6.2831853f;
            float carrier1 = 0.5f + 0.5f * sinf(introPhase);
            float carrier2 = 0.5f + 0.5f * sinf(introPhase * 0.61f + 1.1f);
            float carrier3 = 0.5f + 0.5f * sinf(introPhase * 1.43f + 2.2f);
            float organicLayer = 0.50f * carrier1 + 0.32f * carrier2 + 0.18f * carrier3;

            float centreDrive = (m_introDrive * 0.38f + 0.050f * organicLayer + 0.030f * introEnv)
                              * (0.62f + 0.38f * impulseStrength)
                              * (1.0f + 0.30f * beatEnv);
            float injectionScale = rawDt * 55.0f;

            for (uint16_t k = 0; k < 16; k++) {
                float kernel = expf(-(float)(k * k) * 0.20f);
                float drift = 0.78f + 0.22f * sinf(storyTime * 0.93f - (float)k * 0.17f + 2.0f * fluidBlend);
                float g = centreDrive * kernel * drift * injectionScale;
                u_curr[k] = clampf(u_curr[k] + g, 0.0f, 1.0f);
            }

            float moodDamping = baseDamping * (0.90f + 0.24f * moodNorm + 0.08f * (1.0f - introEnv));

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
                float localDamping = moodDamping
                                   + edgeFactor * (m_edgeAbsorb * (0.82f + 0.18f * layerC));

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

    float ridgeAttack = clampf(rawDt * m_ridgeAttackHz, 0.02f, 1.0f);
    float ridgeRelease = clampf(rawDt * m_ridgeReleaseHz, 0.01f, 1.0f);
    for (uint16_t i = 1; i < kFieldSize - 1; i++) {
        float gL = u_curr[i] - u_curr[i - 1];
        float gR = u_curr[i + 1] - u_curr[i];
        float slope = 0.5f * (fabsf(gL) + fabsf(gR));
        float curvature = fabsf(gR - gL);
        float rawRidge = clampf((0.65f * slope + 1.35f * curvature) * m_ridgeSensitivity, 0.0f, 1.0f);
        rawRidge = powf(rawRidge, 0.85f);
        float env = ridgeEnv[i];
        float a = (rawRidge > env) ? ridgeAttack : ridgeRelease;
        ridgeEnv[i] = env + (rawRidge - env) * a;
    }
    ridgeEnv[0] = ridgeEnv[1];
    ridgeEnv[kFieldSize - 1] = ridgeEnv[kFieldSize - 2];

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

        float layerMix = 0.5f + 0.5f * sinf(storyTime * 0.19f + (float)dist * 0.028f);
        float sampleA = sampleFieldLinear(
            u_curr, kFieldSize,
            (float)fi + 1.2f * sinf(storyTime * 0.31f + (float)dist * 0.050f));
        float sampleB = sampleFieldLinear(
            u_curr, kFieldSize,
            (float)fi + 5.8f + 1.7f * sinf(storyTime * 0.23f - (float)dist * 0.040f + 1.2f));
        float layered = sampleA + (sampleB - sampleA) * layerMix;

        float fieldVal = (layered - normMin) / range;
        fieldVal = clampf(fieldVal, 0.0f, 1.0f);

        float ridge = sampleFieldLinear(
            ridgeEnv, kFieldSize,
            (float)fi + 0.7f * sinf(storyTime * 0.27f + (float)dist * 0.022f));
        ridge = clampf(ridge + m_ridgeFloor * fieldVal, 0.0f, 1.0f);

        float baseSculpt = powf(fieldVal, m_peakGamma);
        float ridgeSculpt = powf(ridge, 0.78f);
        float composite = clampf(
            baseSculpt * (1.0f - 0.28f * m_ridgeBlend) + ridgeSculpt * m_ridgeBlend,
            0.0f, 1.0f);
        uint8_t brightness = static_cast<uint8_t>(composite * static_cast<float>(ctx.brightness));

        uint8_t spatialHue = static_cast<uint8_t>(static_cast<float>(dist) * 0.45f);
        if (spatialHue > 36) spatialHue = 36;
        uint8_t hue = static_cast<uint8_t>(ctx.gHue + chromaHue + spatialHue + reverseHueShift + (uint8_t)(layerMix * 14.0f));

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);

        // Strip 2: reduced offset for stronger coherence, still visually separated.
        float layerMix2 = 0.5f + 0.5f * sinf(storyTime * 0.17f + (float)dist * 0.031f + 1.5f);
        float sample2A = sampleFieldLinear(
            u_curr, kFieldSize,
            (float)fi + 2.4f + 1.0f * sinf(storyTime * 0.29f + (float)dist * 0.043f + 0.7f));
        float sample2B = sampleFieldLinear(
            u_curr, kFieldSize,
            (float)fi + 8.0f + 1.6f * sinf(storyTime * 0.21f - (float)dist * 0.036f + 2.1f));
        float layered2 = sample2A + (sample2B - sample2A) * layerMix2;

        float fieldVal2 = (layered2 - normMin) / range;
        fieldVal2 = clampf(fieldVal2, 0.0f, 1.0f);
        float ridge2 = sampleFieldLinear(
            ridgeEnv, kFieldSize,
            (float)fi + 5.6f + 0.9f * sinf(storyTime * 0.25f - (float)dist * 0.020f));
        ridge2 = clampf(ridge2 + m_ridgeFloor * fieldVal2, 0.0f, 1.0f);

        float baseSculpt2 = powf(fieldVal2, m_peakGamma);
        float ridgeSculpt2 = powf(ridge2, 0.78f);
        float composite2 = clampf(
            baseSculpt2 * (1.0f - 0.28f * m_ridgeBlend) + ridgeSculpt2 * m_ridgeBlend,
            0.0f, 1.0f);
        uint8_t brightness2 = static_cast<uint8_t>(composite2 * static_cast<float>(ctx.brightness));
        uint8_t hue2 = static_cast<uint8_t>(hue + 20 + (uint8_t)(layerMix2 * 10.0f));

        uint16_t s2idx = static_cast<uint16_t>(i + STRIP_LENGTH);
        if (s2idx < ctx.ledCount) {
            ctx.leds[s2idx] = ctx.palette.getColor(hue2, brightness2);
        }
    }
}

void LGPTimeReversalMirrorEffect_Mod3::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPTimeReversalMirrorEffect_Mod3::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Time-Reversal Mirror Mod3",
        "Organic layered time-reversal wave with ridge-locked fang peaks",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPTimeReversalMirrorEffect_Mod3::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPTimeReversalMirrorEffect_Mod3::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPTimeReversalMirrorEffect_Mod3::setParameter(const char* name, float value) {
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
    if (std::strcmp(name, "beat_release_sec") == 0) { m_beatReleaseSec = clampf(value, 0.05f, 2.0f); return true; }
    if (std::strcmp(name, "normalise_follow_hz") == 0) {
        m_normaliseFollowHz = clampf(value, 0.5f, 20.0f);
        return true;
    }
    if (std::strcmp(name, "peak_gamma") == 0) { m_peakGamma = clampf(value, 0.5f, 3.0f); return true; }
    if (std::strcmp(name, "ridge_attack_hz") == 0) { m_ridgeAttackHz = clampf(value, 0.5f, 60.0f); return true; }
    if (std::strcmp(name, "ridge_release_hz") == 0) { m_ridgeReleaseHz = clampf(value, 0.5f, 30.0f); return true; }
    if (std::strcmp(name, "ridge_sensitivity") == 0) { m_ridgeSensitivity = clampf(value, 0.2f, 10.0f); return true; }
    if (std::strcmp(name, "ridge_blend") == 0) { m_ridgeBlend = clampf(value, 0.0f, 1.0f); return true; }
    if (std::strcmp(name, "ridge_floor") == 0) { m_ridgeFloor = clampf(value, 0.0f, 0.5f); return true; }
    return false;
}

float LGPTimeReversalMirrorEffect_Mod3::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (std::strcmp(name, "csq") == 0) return m_csq;
    if (std::strcmp(name, "damping") == 0) return m_damping;
    if (std::strcmp(name, "edge_absorb") == 0) return m_edgeAbsorb;
    if (std::strcmp(name, "impulse_every") == 0) return static_cast<float>(m_impulseEvery);
    if (std::strcmp(name, "forward_sec") == 0) return m_forwardSec;
    if (std::strcmp(name, "reverse_sec") == 0) return m_reverseSec;
    if (std::strcmp(name, "intro_sec") == 0) return m_introSec;
    if (std::strcmp(name, "intro_drive") == 0) return m_introDrive;
    if (std::strcmp(name, "beat_release_sec") == 0) return m_beatReleaseSec;
    if (std::strcmp(name, "normalise_follow_hz") == 0) return m_normaliseFollowHz;
    if (std::strcmp(name, "peak_gamma") == 0) return m_peakGamma;
    if (std::strcmp(name, "ridge_attack_hz") == 0) return m_ridgeAttackHz;
    if (std::strcmp(name, "ridge_release_hz") == 0) return m_ridgeReleaseHz;
    if (std::strcmp(name, "ridge_sensitivity") == 0) return m_ridgeSensitivity;
    if (std::strcmp(name, "ridge_blend") == 0) return m_ridgeBlend;
    if (std::strcmp(name, "ridge_floor") == 0) return m_ridgeFloor;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
