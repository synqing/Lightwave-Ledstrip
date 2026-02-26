/**
 * @file LGPTimeReversalMirrorEffect.cpp
 * @brief LGP Time-Reversal Mirror effect implementation
 *
 * 1D damped wave equation recorded in a PSRAM history ring buffer then
 * played back in reverse with a phase flip -- "physics rewinding inside glass."
 *
 * Centre-origin compliant: field is indexed by centerPairDistance(i), so
 * wave propagation radiates outward from LEDs 79/80.
 */

#include "LGPTimeReversalMirrorEffect.h"
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
    {"csq", "Wave Propagation", 0.01f, 0.40f, 0.15f,
     plugins::EffectParameterType::FLOAT, 0.005f, "wave", "", false},
    {"damping", "Damping", 0.005f, 0.20f, 0.04f,
     plugins::EffectParameterType::FLOAT, 0.002f, "wave", "", false},
    {"impulse_every", "Impulse Every", 12.0f, 240.0f, 90.0f,
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

LGPTimeReversalMirrorEffect::LGPTimeReversalMirrorEffect()
    : m_ps(nullptr)
    , m_phaseTimer(0.0f)
    , m_isReverse(false)
    , m_frameInPhase(0)
    , m_historyWrite(0)
    , m_historyCount(0)
    , m_historyRead(0)
    , m_frameSinceImpulse(0)
    , m_fallbackPhase(0.0f)
{
}

// =========================================================================
// init()
// =========================================================================

bool LGPTimeReversalMirrorEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Allocate large field + history buffers in PSRAM
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPTimeReversalMirror: PSRAM alloc failed (%u bytes)",
                     (unsigned)sizeof(PsramData));
            return false;
        }
    }
    memset(m_ps, 0, sizeof(PsramData));
#else
    // Native build stub -- no PSRAM
    m_ps = nullptr;
#endif

    // Reset phase state
    m_phaseTimer       = 0.0f;
    m_isReverse        = false;
    m_frameInPhase     = 0;
    m_historyWrite     = 0;
    m_historyCount     = 0;
    m_historyRead      = 0;
    m_frameSinceImpulse = 0;
    m_fallbackPhase    = 0.0f;

    // Seed the field with a gentle centre bump to avoid a dead start
    if (m_ps) {
        for (uint16_t i = 0; i < kFieldSize; i++) {
            float distNorm = (float)i / (float)(kFieldSize - 1);
            float bump = expf(-distNorm * distNorm * 20.0f) * 0.3f;
            m_ps->u_curr[i] = 0.5f + bump;
            m_ps->u_prev[i] = 0.5f;
        }
    }

#if FEATURE_AUDIO_SYNC
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaSmoothed[i] = 0.0f;
        m_chromaTargets[i]  = 0.0f;
    }
    m_chromaAngle = 0.0f;
    m_rmsFollower.reset(0.0f);
    m_targetRms   = 0.0f;
    m_lastHopSeq  = 0;
#endif

    return true;
}

// =========================================================================
// render()
// =========================================================================

void LGPTimeReversalMirrorEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;  // PSRAM guard

    // -----------------------------------------------------------------
    // Timing
    // -----------------------------------------------------------------
    float rawDt     = ctx.getSafeRawDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float moodNorm  = ctx.getMoodNormalized();

    // -----------------------------------------------------------------
    // Audio processing
    // -----------------------------------------------------------------
    float impulseStrength = 0.6f;   // Default impulse strength (no audio)
    uint8_t chromaHue     = 0;

#if FEATURE_AUDIO_SYNC
    bool beatTriggered    = false;
    if (ctx.audio.available) {
        // Hop-gated target updates
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            m_targetRms  = ctx.audio.rms();

            for (uint8_t i = 0; i < 12; i++) {
                m_chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }
        }

        // Smooth toward targets every frame
        float smoothedRms = m_rmsFollower.updateWithMood(m_targetRms, rawDt, moodNorm);

        for (uint8_t i = 0; i < 12; i++) {
            m_chromaSmoothed[i] = m_chromaFollowers[i].updateWithMood(
                m_chromaTargets[i], rawDt, moodNorm);
        }

        chromaHue = effects::chroma::circularChromaHueSmoothed(
            m_chromaSmoothed, m_chromaAngle, rawDt, 0.20f);

        // RMS modulates impulse strength (0.3 .. 1.0)
        impulseStrength = 0.3f + 0.7f * clampf(smoothedRms * 2.0f, 0.0f, 1.0f);

        // Beat triggers extra impulse in forward phase
        beatTriggered = ctx.audio.isOnBeat();
    } else {
        // No audio available -- gentle fallback
        m_fallbackPhase += speedNorm * 0.4f * rawDt;
        if (m_fallbackPhase > 6.2831853f) m_fallbackPhase -= 6.2831853f;
        chromaHue = (uint8_t)(m_fallbackPhase * (255.0f / 6.2831853f));
    }
#else
    // Audio feature disabled -- fallback animation
    m_fallbackPhase += speedNorm * 0.4f * rawDt;
    if (m_fallbackPhase > 6.2831853f) m_fallbackPhase -= 6.2831853f;
    chromaHue = (uint8_t)(m_fallbackPhase * (255.0f / 6.2831853f));
#endif

    // -----------------------------------------------------------------
    // Phase machine: forward vs reverse
    // -----------------------------------------------------------------
    m_phaseTimer += rawDt;
    m_frameInPhase++;

    if (!m_isReverse) {
        // === FORWARD PHASE ===
        float forwardDur = m_forwardSec / fmaxf(speedNorm, 0.2f);
        if (m_phaseTimer >= forwardDur) {
            // Transition to reverse
            m_isReverse    = true;
            m_phaseTimer   = 0.0f;
            m_frameInPhase = 0;
            // Set read cursor to last written snapshot
            m_historyRead  = (int16_t)m_historyCount - 1;
        } else {
            // -------------------------------------------------------
            // Wave equation step (1D damped wave, Neumann boundaries)
            // -------------------------------------------------------
            // Inject centre impulse periodically (and on beat)
            m_frameSinceImpulse++;
            bool doImpulse = (m_frameSinceImpulse >= m_impulseEvery);
#if FEATURE_AUDIO_SYNC
            if (beatTriggered && !m_isReverse) doImpulse = true;
#endif
            if (doImpulse) {
                m_frameSinceImpulse = 0;
                // Gaussian impulse centred at field[0] (= strip centre)
                for (uint16_t k = 0; k < 8; k++) {
                    float g = expf(-(float)(k * k) * 0.5f) * impulseStrength * 0.25f;
                    m_ps->u_curr[k] += g;
                    m_ps->u_curr[k] = clampf(m_ps->u_curr[k], 0.0f, 1.0f);
                }
            }

            // Compute u_next from wave equation
            for (uint16_t i = 0; i < kFieldSize; i++) {
                // Laplacian with Neumann (reflecting) boundary conditions
                float left  = (i > 0)              ? m_ps->u_curr[i - 1] : m_ps->u_curr[0];
                float right = (i < kFieldSize - 1) ? m_ps->u_curr[i + 1] : m_ps->u_curr[kFieldSize - 1];
                float laplacian = left - 2.0f * m_ps->u_curr[i] + right;

                m_ps->u_next[i] = 2.0f * m_ps->u_curr[i]
                                 - m_ps->u_prev[i]
                                 + m_csq * laplacian
                                 - m_damping * m_ps->u_curr[i];

                // Clamp to prevent divergence
                m_ps->u_next[i] = clampf(m_ps->u_next[i], -0.5f, 1.5f);
            }

            // Rotate buffers
            memcpy(m_ps->u_prev, m_ps->u_curr, sizeof(float) * kFieldSize);
            memcpy(m_ps->u_curr, m_ps->u_next, sizeof(float) * kFieldSize);

            // Record snapshot into history ring buffer
            if (m_historyWrite < kHistoryDepth) {
                memcpy(m_ps->history[m_historyWrite], m_ps->u_curr,
                       sizeof(float) * kFieldSize);
                m_historyWrite++;
                if (m_historyCount < m_historyWrite) {
                    m_historyCount = m_historyWrite;
                }
            }
        }
    } else {
        // === REVERSE PHASE ===
        float reverseDur = m_reverseSec / fmaxf(speedNorm, 0.2f);
        if (m_phaseTimer >= reverseDur || m_historyRead < 0) {
            // Transition back to forward
            m_isReverse    = false;
            m_phaseTimer   = 0.0f;
            m_frameInPhase = 0;
            m_historyWrite = 0;
            m_historyCount = 0;
            m_frameSinceImpulse = 0;

            // Re-seed field for next forward pass
            for (uint16_t i = 0; i < kFieldSize; i++) {
                float distNorm = (float)i / (float)(kFieldSize - 1);
                float bump = expf(-distNorm * distNorm * 20.0f) * 0.3f;
                m_ps->u_curr[i] = 0.5f + bump;
                m_ps->u_prev[i] = 0.5f;
            }
        } else {
            // Read history backwards; step read cursor by speed
            // (skip frames at high speed to fill the time window)
            uint16_t step = 1;
            if (m_historyCount > 0) {
                float idealSteps = (float)m_historyCount / (reverseDur / fmaxf(rawDt, 0.001f));
                step = (uint16_t)fmaxf(1.0f, idealSteps);
            }
            m_historyRead -= (int16_t)step;
            if (m_historyRead < 0) m_historyRead = 0;

            // Load reversed snapshot into u_curr with phase flip (invert around 0.5)
            const float* snap = m_ps->history[m_historyRead];
            for (uint16_t i = 0; i < kFieldSize; i++) {
                m_ps->u_curr[i] = 1.0f - snap[i];  // Phase flip
            }
        }
    }

    // -----------------------------------------------------------------
    // Render: map field to LEDs via centre-origin distance
    // -----------------------------------------------------------------

    // Compute normalisation range of the current field
    float fieldMin = m_ps->u_curr[0];
    float fieldMax = m_ps->u_curr[0];
    for (uint16_t i = 1; i < kFieldSize; i++) {
        if (m_ps->u_curr[i] < fieldMin) fieldMin = m_ps->u_curr[i];
        if (m_ps->u_curr[i] > fieldMax) fieldMax = m_ps->u_curr[i];
    }
    float fieldRange = fieldMax - fieldMin;
    if (fieldRange < 0.01f) fieldRange = 0.01f;  // Avoid division by zero

    // Reverse-phase visual indicator: slight hue shift
    uint8_t reverseHueShift = m_isReverse ? 20 : 0;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        // Clamp to field size (distances 0..79)
        uint16_t fi = (dist < kFieldSize) ? dist : (kFieldSize - 1);

        // Normalise field value to 0..1
        float fieldVal = (m_ps->u_curr[fi] - fieldMin) / fieldRange;
        fieldVal = clampf(fieldVal, 0.0f, 1.0f);

        // Brightness from field value, scaled by master brightness
        uint8_t brightness = (uint8_t)(fieldVal * (float)ctx.brightness);

        // Hue: base chroma hue + small spatial offset (max ~40 hue units)
        // No rainbow -- two-tone palette shift capped at 40 units
        uint8_t spatialHue = (uint8_t)((float)dist * 0.5f);  // 0..39 over 80 LEDs
        if (spatialHue > 40) spatialHue = 40;
        uint8_t hue = (uint8_t)(ctx.gHue + chromaHue + spatialHue + reverseHueShift);

        // Strip 1
        ctx.leds[i] = ctx.palette.getColor(hue, brightness);

        // Strip 2: slight phase offset (+10 field index, clamped) and hue +30
        uint16_t fi2 = (fi + 10 < kFieldSize) ? (fi + 10) : (kFieldSize - 1);
        float fieldVal2 = (m_ps->u_curr[fi2] - fieldMin) / fieldRange;
        fieldVal2 = clampf(fieldVal2, 0.0f, 1.0f);
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

void LGPTimeReversalMirrorEffect::cleanup() {
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

const plugins::EffectMetadata& LGPTimeReversalMirrorEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Time-Reversal Mirror",
        "Damped wave recorded then replayed in reverse with phase flip",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPTimeReversalMirrorEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPTimeReversalMirrorEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPTimeReversalMirrorEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (std::strcmp(name, "csq") == 0) {
        m_csq = clampf(value, 0.01f, 0.40f);
        return true;
    }
    if (std::strcmp(name, "damping") == 0) {
        m_damping = clampf(value, 0.005f, 0.20f);
        return true;
    }
    if (std::strcmp(name, "impulse_every") == 0) {
        m_impulseEvery = static_cast<uint16_t>(clampf(value, 12.0f, 240.0f) + 0.5f);
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

float LGPTimeReversalMirrorEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (std::strcmp(name, "csq") == 0) return m_csq;
    if (std::strcmp(name, "damping") == 0) return m_damping;
    if (std::strcmp(name, "impulse_every") == 0) return static_cast<float>(m_impulseEvery);
    if (std::strcmp(name, "forward_sec") == 0) return m_forwardSec;
    if (std::strcmp(name, "reverse_sec") == 0) return m_reverseSec;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
