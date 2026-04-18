/**
 * @file LGPTimeReversalMirrorEffect_AR.cpp
 * @brief LGP Time-Reversal Mirror (AR) - Audio-reactive variant
 *
 * Faithful rewrite based on the original LGPTimeReversalMirrorEffect.cpp
 * rendering pipeline with layered AR enhancements:
 *   - Kick envelope (isOnBeat) modulates impulse strength and triggers extra impulses
 *   - Snare (isSnareHit) can trigger early reverse transition (rate-limited)
 *   - Speed controls phase durations and impulse cadence
 *   - Mood controls wave damping and AsymmetricFollower smoothing
 *
 * Centre-origin compliant: field indexed by centerPairDistance(i), so
 * wave propagation radiates outward from LEDs 79/80.
 *
 * ALL rendering uses ctx.palette.getColor(hue, brightness) with
 * dynamic per-frame min/max normalisation and linear brightness mapping.
 * No fadeToBlackBy, no nscale8_video, no gamma, no soft limiter.
 */

#include "LGPTimeReversalMirrorEffect_AR.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
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
    {"csq", "Wave Propagation", 0.01f, 0.40f, 0.15f,
     plugins::EffectParameterType::FLOAT, 0.005f, "wave", "", false},
    {"base_damping", "Base Damping", 0.005f, 0.20f, 0.04f,
     plugins::EffectParameterType::FLOAT, 0.002f, "wave", "", false},
    {"base_impulse_every", "Base Impulse Every", 12.0f, 240.0f, 90.0f,
     plugins::EffectParameterType::INT, 1.0f, "timing", "frames", false},
    {"forward_sec", "Forward Seconds", 1.0f, 20.0f, 4.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "s", false},
    {"reverse_sec", "Reverse Seconds", 0.5f, 20.0f, 2.5f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "s", false},
};
}

// =========================================================================
// Helpers
// =========================================================================

static inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

// =========================================================================
// Construction
// =========================================================================

LGPTimeReversalMirrorEffect_AR::LGPTimeReversalMirrorEffect_AR() = default;

// =========================================================================
// init()
// =========================================================================

bool LGPTimeReversalMirrorEffect_AR::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Allocate large field + history buffers in PSRAM
#ifndef NATIVE_BUILD
    const bool wasFirstAlloc = (m_ps == nullptr);
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPTimeReversalMirror_AR: PSRAM alloc failed (%u bytes)",
                     (unsigned)sizeof(PsramData));
            return false;
        }
    }
    // Only zero live field arrays on re-init; history remains gated by the
    // per-zone history counts so we preserve the existing lifecycle policy.
    if (wasFirstAlloc) {
        memset(m_ps, 0, sizeof(PsramData));
    } else {
        memset(m_ps->u_prev, 0, sizeof(m_ps->u_prev));
        memset(m_ps->u_curr, 0, sizeof(m_ps->u_curr));
        memset(m_ps->u_next, 0, sizeof(m_ps->u_next));
    }
#else
    // Native build stub -- no PSRAM
    m_ps = nullptr;
#endif

    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_phaseTimer[zi] = 0.0f;
        m_isReverse[zi] = false;
        m_frameInPhase[zi] = 0;
        m_historyWrite[zi] = 0;
        m_historyCount[zi] = 0;
        m_historyRead[zi] = 0;
        m_frameSinceImpulse[zi] = 0;
        m_fallbackPhase[zi] = 0.0f;
        m_kickEnv[zi] = 0.0f;
        m_snareEnv[zi] = 0.0f;
        m_lastReverseMs[zi] = 0;

        // Seed every zone slot with the same gentle centre bump so zone
        // reassignments do not inherit another zone's recorded field.
        if (m_ps) {
            for (uint16_t i = 0; i < kFieldSize; i++) {
                float distNorm = (float)i / (float)(kFieldSize - 1);
                float bump = expf(-distNorm * distNorm * 20.0f) * 0.3f;
                m_ps->u_curr[zi][i] = 0.5f + bump;
                m_ps->u_prev[zi][i] = 0.5f;
                m_ps->u_next[zi][i] = 0.0f;
            }
        }
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

// =========================================================================
// render()
// =========================================================================

void LGPTimeReversalMirrorEffect_AR::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;  // PSRAM guard
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
    int16_t& historyRead = m_historyRead[z];
    uint16_t& frameSinceImpulse = m_frameSinceImpulse[z];
    float& fallbackPhase = m_fallbackPhase[z];
    float& kickEnv = m_kickEnv[z];
    float& snareEnv = m_snareEnv[z];
    uint32_t& lastReverseMs = m_lastReverseMs[z];

    // -----------------------------------------------------------------
    // Timing and normalised controls
    // -----------------------------------------------------------------
    float rawDt     = ctx.getSafeRawDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float moodNorm  = ctx.getMoodNormalized();

    // -----------------------------------------------------------------
    // AR: Mood-modulated damping
    //   mood=0 -> less damped (longer decay), mood=1 -> more damped (tighter)
    // -----------------------------------------------------------------
    float damping = m_baseDamping * (0.6f + 0.8f * moodNorm);

    // -----------------------------------------------------------------
    // AR: Speed-modulated impulse cadence
    //   Higher speed -> more frequent impulses
    // -----------------------------------------------------------------
    uint16_t impulseEvery = (uint16_t)(
        (float)m_baseImpulseEvery / fmaxf(speedNorm, 0.3f));
    if (impulseEvery < 1) impulseEvery = 1;

    // -----------------------------------------------------------------
    // AR: Envelope decay (every frame, regardless of audio)
    // -----------------------------------------------------------------
    kickEnv *= expf(-rawDt / 0.15f);
    snareEnv *= expf(-rawDt / 0.20f);

    // -----------------------------------------------------------------
    // Audio processing
    // -----------------------------------------------------------------
    float impulseStrength = 0.6f;   // Default (no audio)
    uint8_t chromaHue     = 0;

#if FEATURE_AUDIO_SYNC
    bool beatTriggered = false;
    bool snareHit      = false;

    float* chromaSmoothed = m_chromaSmoothed[z];
    float* chromaTargets = m_chromaTargets[z];
    enhancement::AsymmetricFollower* chromaFollowers = m_chromaFollowers[z];
    float& chromaAngle = m_chromaAngle[z];
    enhancement::AsymmetricFollower& rmsFollower = m_rmsFollower[z];
    float& targetRms = m_targetRms[z];
    uint32_t& lastHopSeq = m_lastHopSeq[z];

    if (ctx.audio.available) {
        // Hop-gated target updates
        bool newHop = (ctx.audio.hopSequence() != lastHopSeq);
        if (newHop) {
            lastHopSeq = ctx.audio.hopSequence();
            targetRms = ctx.audio.rms();

            for (uint8_t i = 0; i < 12; i++) {
                chromaTargets[i] = ctx.audio.getHeavyChroma(i);
            }
        }

        // Smooth toward targets every frame
        float smoothedRms = rmsFollower.updateWithMood(targetRms, rawDt, moodNorm);

        for (uint8_t i = 0; i < 12; i++) {
            chromaSmoothed[i] = chromaFollowers[i].updateWithMood(
                chromaTargets[i], rawDt, moodNorm);
        }

        chromaHue = effects::chroma::circularChromaHueSmoothed(
            chromaSmoothed, chromaAngle, rawDt, 0.20f);

        // RMS modulates impulse strength (0.3 .. 1.0)
        impulseStrength = 0.3f + 0.7f * clampf(smoothedRms * 2.0f, 0.0f, 1.0f);

        // Beat and snare detection
        beatTriggered = ctx.audio.isOnBeat();
        snareHit      = ctx.audio.isSnareHit();

        // AR: Kick envelope -- fast attack from beat
        if (beatTriggered) {
            kickEnv = fmaxf(kickEnv, 0.85f);
        }

        // AR: Snare envelope -- fast attack from snare hit
        if (snareHit) {
            snareEnv = fmaxf(snareEnv, 0.7f);
        }

        // AR: Kick envelope modulates impulse strength
        impulseStrength *= (0.7f + 0.3f * kickEnv);

    } else {
        // No audio available -- gentle fallback hue rotation
        fallbackPhase += speedNorm * 0.4f * rawDt;
        if (fallbackPhase > 6.2831853f) fallbackPhase -= 6.2831853f;
        chromaHue = (uint8_t)(fallbackPhase * (255.0f / 6.2831853f));
    }
#else
    // Audio feature disabled -- fallback animation
    fallbackPhase += speedNorm * 0.4f * rawDt;
    if (fallbackPhase > 6.2831853f) fallbackPhase -= 6.2831853f;
    chromaHue = (uint8_t)(fallbackPhase * (255.0f / 6.2831853f));
#endif

    // -----------------------------------------------------------------
    // Phase machine: forward vs reverse
    // -----------------------------------------------------------------
    phaseTimer += rawDt;
    frameInPhase++;

    if (!isReverse) {
        // === FORWARD PHASE ===
        float forwardDur = m_forwardSec / fmaxf(speedNorm, 0.2f);

        // AR: Snare can trigger early reverse (rate-limited by cooldown)
#if FEATURE_AUDIO_SYNC
        bool snareTriggeredReverse = false;
        if (snareHit && ctx.audio.available) {
            uint32_t nowMs = (uint32_t)(millis());
            uint32_t cooldownMs = (uint32_t)(kMinReverseCooldownSec * 1000.0f);
            if ((nowMs - lastReverseMs) > cooldownMs && historyCount > 0) {
                snareTriggeredReverse = true;
            }
        }
#endif

        bool phaseExpired = (phaseTimer >= forwardDur);

#if FEATURE_AUDIO_SYNC
        if (phaseExpired || snareTriggeredReverse) {
#else
        if (phaseExpired) {
#endif
            // Transition to reverse
            isReverse = true;
            phaseTimer = 0.0f;
            frameInPhase = 0;
            // Set read cursor to last written snapshot
            historyRead = (int16_t)historyCount - 1;
            lastReverseMs = (uint32_t)(millis());
        } else {
            // -------------------------------------------------------
            // Wave equation step (1D damped wave, Neumann boundaries)
            // -------------------------------------------------------

            // Inject centre impulse periodically (and on beat/kick)
            frameSinceImpulse++;
            bool doImpulse = (frameSinceImpulse >= impulseEvery);

#if FEATURE_AUDIO_SYNC
            // Beat triggers extra impulse during forward phase
            if (beatTriggered) doImpulse = true;
#endif

            if (doImpulse) {
                frameSinceImpulse = 0;
                // Gaussian impulse centred at field[0] (= strip centre)
                for (uint16_t k = 0; k < 8; k++) {
                    float g = expf(-(float)(k * k) * 0.5f) * impulseStrength * 0.25f;
                    u_curr[k] += g;
                    u_curr[k] = clampf(u_curr[k], 0.0f, 1.0f);
                }
            }

            // Compute u_next from wave equation
            for (uint16_t i = 0; i < kFieldSize; i++) {
                // Laplacian with Neumann (reflecting) boundary conditions
                float left  = (i > 0) ? u_curr[i - 1] : u_curr[0];
                float right = (i < kFieldSize - 1) ? u_curr[i + 1] : u_curr[kFieldSize - 1];
                float laplacian = left - 2.0f * u_curr[i] + right;

                u_next[i] = 2.0f * u_curr[i]
                                 - u_prev[i]
                                 + m_csq * laplacian
                                 - damping * u_curr[i];

                // Clamp to prevent divergence
                u_next[i] = clampf(u_next[i], -0.5f, 1.5f);
            }

            // Rotate buffers
            memcpy(u_prev, u_curr, sizeof(float) * kFieldSize);
            memcpy(u_curr, u_next, sizeof(float) * kFieldSize);

            // Record snapshot into history buffer
            if (historyWrite < kHistoryDepth) {
                memcpy(history[historyWrite], u_curr,
                       sizeof(float) * kFieldSize);
                historyWrite++;
                if (historyCount < historyWrite) {
                    historyCount = historyWrite;
                }
            }
        }
    } else {
        // === REVERSE PHASE ===
        float reverseDur = m_reverseSec / fmaxf(speedNorm, 0.2f);
        if (phaseTimer >= reverseDur || historyRead < 0) {
            // Transition back to forward
            isReverse = false;
            phaseTimer = 0.0f;
            frameInPhase = 0;
            historyWrite = 0;
            historyCount = 0;
            frameSinceImpulse = 0;

            // Re-seed field for next forward pass
            for (uint16_t i = 0; i < kFieldSize; i++) {
                float distNorm = (float)i / (float)(kFieldSize - 1);
                float bump = expf(-distNorm * distNorm * 20.0f) * 0.3f;
                u_curr[i] = 0.5f + bump;
                u_prev[i] = 0.5f;
                u_next[i] = 0.0f;
            }
        } else {
            // Read history backwards; step read cursor by speed
            // (skip frames at high speed to fill the time window)
            uint16_t step = 1;
            if (historyCount > 0) {
                float idealSteps = (float)historyCount / (reverseDur / fmaxf(rawDt, 0.001f));
                step = (uint16_t)fmaxf(1.0f, idealSteps);
            }
            historyRead -= (int16_t)step;
            if (historyRead < 0) historyRead = 0;

            // Load reversed snapshot into u_curr with phase flip (invert around 0.5)
            const float* snap = history[historyRead];
            for (uint16_t i = 0; i < kFieldSize; i++) {
                u_curr[i] = 1.0f - snap[i];  // Phase flip
            }
        }
    }

    // -----------------------------------------------------------------
    // Render: map field to LEDs via centre-origin distance
    // -----------------------------------------------------------------

    // Compute normalisation range of the current field
    float fieldMin = u_curr[0];
    float fieldMax = u_curr[0];
    for (uint16_t i = 1; i < kFieldSize; i++) {
        if (u_curr[i] < fieldMin) fieldMin = u_curr[i];
        if (u_curr[i] > fieldMax) fieldMax = u_curr[i];
    }
    float fieldRange = fieldMax - fieldMin;
    if (fieldRange < 0.01f) fieldRange = 0.01f;  // Avoid division by zero

    // Reverse-phase visual indicator: slight hue shift
    uint8_t reverseHueShift = isReverse ? 20 : 0;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        // Clamp to field size (distances 0..79)
        uint16_t fi = (dist < kFieldSize) ? dist : (kFieldSize - 1);

        // Normalise field value to 0..1
        float fieldVal = (u_curr[fi] - fieldMin) / fieldRange;
        fieldVal = clampf(fieldVal, 0.0f, 1.0f);

        // AR: During reverse phase, snare envelope provides a subtle brightness boost
        if (isReverse) {
            fieldVal *= (1.0f + 0.15f * snareEnv);
            fieldVal = clampf(fieldVal, 0.0f, 1.0f);
        }

        // Brightness from field value, scaled by master brightness (linear)
        uint8_t brightness = (uint8_t)(fieldVal * (float)ctx.brightness);

        // Hue: base chroma hue + small spatial offset (max ~40 hue units)
        // No rainbow -- two-tone palette shift capped at 40 units
        uint8_t spatialHue = (uint8_t)((float)dist * 0.5f);  // 0..39 over 80 LEDs
        if (spatialHue > 40) spatialHue = 40;
        uint8_t hue = (uint8_t)(ctx.gHue + chromaHue + spatialHue + reverseHueShift);

        // Strip A
        ctx.leds[i] = ctx.palette.getColor(hue, brightness);

        // Strip B: slight phase offset (+10 field index, clamped) and hue +30
        uint16_t fi2 = (fi + 10 < kFieldSize) ? (fi + 10) : (kFieldSize - 1);
        float fieldVal2 = (u_curr[fi2] - fieldMin) / fieldRange;
        fieldVal2 = clampf(fieldVal2, 0.0f, 1.0f);

        // AR: Same snare brightness boost for Strip B
        if (isReverse) {
            fieldVal2 *= (1.0f + 0.15f * snareEnv);
            fieldVal2 = clampf(fieldVal2, 0.0f, 1.0f);
        }

        uint8_t brightness2 = (uint8_t)(fieldVal2 * (float)ctx.brightness);
        uint8_t hue2 = (uint8_t)(hue + 30);

        uint16_t s2idx = i + STRIP_LENGTH;
        if (s2idx < ctx.ledCount) {
            ctx.leds[s2idx] = ctx.palette.getColor(hue2, brightness2);
        }
    }
}

// =========================================================================
// cleanup()
// =========================================================================

void LGPTimeReversalMirrorEffect_AR::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

// =========================================================================
// getMetadata()
// =========================================================================

const plugins::EffectMetadata& LGPTimeReversalMirrorEffect_AR::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Time-Reversal Mirror (AR)",
        "Audio-reactive wave recorder with kick impulses and snare-triggered reverse",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPTimeReversalMirrorEffect_AR::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPTimeReversalMirrorEffect_AR::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPTimeReversalMirrorEffect_AR::setParameter(const char* name, float value) {
    if (!name) return false;
    if (std::strcmp(name, "csq") == 0) {
        m_csq = clampf(value, 0.01f, 0.40f);
        return true;
    }
    if (std::strcmp(name, "base_damping") == 0) {
        m_baseDamping = clampf(value, 0.005f, 0.20f);
        return true;
    }
    if (std::strcmp(name, "base_impulse_every") == 0) {
        m_baseImpulseEvery = static_cast<uint16_t>(clampf(value, 12.0f, 240.0f) + 0.5f);
        return true;
    }
    if (std::strcmp(name, "forward_sec") == 0) {
        m_forwardSec = clampf(value, 1.0f, 20.0f);
        return true;
    }
    if (std::strcmp(name, "reverse_sec") == 0) {
        m_reverseSec = clampf(value, 0.5f, 20.0f);
        return true;
    }
    return false;
}

float LGPTimeReversalMirrorEffect_AR::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (std::strcmp(name, "csq") == 0) return m_csq;
    if (std::strcmp(name, "base_damping") == 0) return m_baseDamping;
    if (std::strcmp(name, "base_impulse_every") == 0) return static_cast<float>(m_baseImpulseEvery);
    if (std::strcmp(name, "forward_sec") == 0) return m_forwardSec;
    if (std::strcmp(name, "reverse_sec") == 0) return m_reverseSec;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
