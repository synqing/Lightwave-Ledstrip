/**
 * @file LGPFresnelCausticReactiveEffect.cpp
 * @brief LGP Fresnel Caustic Reactive -- "Chromatic Resonance" multi-harmonic oscillator bank
 *
 * Focus position = weighted sum of 4 sine oscillators at incommensurate
 * frequencies. Audio modulates the AMPLITUDE ENVELOPE of each oscillator,
 * never the position directly. The result is always smooth (sum of sines)
 * but musically responsive and never repetitive.
 *
 * Oscillator frequencies (rad/s):
 *   0: 0.3  -- glacial sway          (bass-weighted)
 *   1: 0.7  -- moderate breathing     (bass-weighted)
 *   2: 1.1  -- mid-tempo drift        (treble-weighted)
 *   3: 1.9  -- fast shimmer           (treble-weighted, beat-burst target)
 *
 * These are mutually incommensurate (no small integer ratios), so the
 * combined waveform has no short-period repeat -- the motion always looks
 * organic and non-mechanical.
 *
 * Audio coupling:
 *   - Bass energy scales amplitudes of oscillators 0 and 1
 *   - Treble/RMS energy scales amplitudes of oscillators 2 and 3
 *   - Beat detection injects a decaying burst into oscillator 3's amplitude
 *   - Overall energy scales the sum (loud = wide sweep, quiet = narrow)
 *   - Ring phase speed couples to energy (loud = faster breathing)
 *   - Fade persistence couples to energy squared (loud = crisp short trails)
 *
 * Fresnel geometry is identical to 0x1B04 (Sweep). Dual-strip parallax
 * preserved. Chroma-driven colour preserved.
 */

#include "LGPFresnelCausticReactiveEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace {

// Tunable parameters (wired into render)
float gSpeedScale  = 1.0f;
float gOutputGain  = 1.0f;
float gCentreBias  = 1.0f;

const lightwaveos::plugins::EffectParameter kParams[] = {
    {"fresnel_reactive_speed_scale", "Speed Scale", 0.25f, 2.0f, 1.0f, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"fresnel_reactive_output_gain", "Output Gain", 0.25f, 2.0f, 1.0f, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"fresnel_reactive_centre_bias", "Centre Bias", 0.50f, 1.50f, 1.0f, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};

} // namespace

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ---------------------------------------------------------------------------
// Constants (same Fresnel geometry as 0x1B04)
// ---------------------------------------------------------------------------

static constexpr float kTwoPi = 6.2831853f;
static constexpr float MAX_D = 80.0f;
static constexpr float CORE_SLOPE = 85.0f;
static constexpr uint8_t RING_SPATIAL_FREQ = 18;
static constexpr uint8_t RING_SUPPRESS = 90;
static constexpr uint8_t CORE_GAIN = 200;

// ---------------------------------------------------------------------------
// Oscillator bank frequencies (rad/s) -- mutually incommensurate
// ---------------------------------------------------------------------------
static constexpr float OSC_FREQ[4] = { 0.30f, 0.70f, 1.10f, 1.90f };

// Base amplitude for each oscillator when audio is absent (fallback mode).
static constexpr float OSC_BASE_AMP[4] = { 0.35f, 0.25f, 0.15f, 0.10f };

// Audio amplitude scaling per oscillator:
// Oscillators 0-1 are bass-coupled, 2-3 are treble-coupled
static constexpr float OSC_BASS_WEIGHT[4]   = { 0.60f, 0.40f, 0.10f, 0.05f };
static constexpr float OSC_TREBLE_WEIGHT[4] = { 0.05f, 0.10f, 0.40f, 0.60f };

// Beat burst: amplitude injected into each oscillator on beat detection.
static constexpr float OSC_BEAT_BURST[4] = { 0.05f, 0.10f, 0.25f, 0.50f };

// Amplitude envelope smoothing per oscillator.
static constexpr float OSC_AMP_SMOOTH[4] = { 0.03f, 0.05f, 0.08f, 0.12f };

// Energy followers
static constexpr float ENERGY_SLOW_ALPHA  = 0.04f;
static constexpr float PEAK_ATTACK        = 0.7f;
static constexpr float PEAK_RELEASE       = 0.12f;
static constexpr float BASS_SMOOTH_ALPHA  = 0.08f;
static constexpr float TREBLE_SMOOTH_ALPHA = 0.10f;

// Focus position: minimum offset from centre
static constexpr float FOCUS_MIN_OFFSET = 0.08f;

// Beat burst decay rate (per frame at 60fps)
static constexpr float BEAT_BURST_DECAY = 0.85f;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

LGPFresnelCausticReactiveEffect::LGPFresnelCausticReactiveEffect()
    : m_oscPhase{}
    , m_oscAmplitude{}
    , m_energySmooth(0.0f)
    , m_energyPeak(0.0f)
    , m_bassSmooth(0.0f)
    , m_trebleSmooth(0.0f)
    , m_beatBurst(0.0f)
    , m_ringPhase(0.0f)
    , m_chromaAngle(0.0f)
    , m_beatFlash(0.0f)
    , m_fallbackPhase(0.0f)
#if FEATURE_AUDIO_SYNC
    , m_lastHopSeq(0)
    , m_chromaSmoothed{}
#endif
{
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

bool LGPFresnelCausticReactiveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    gSpeedScale = 1.0f;
    gOutputGain = 1.0f;
    gCentreBias = 1.0f;

    for (uint8_t k = 0; k < NUM_OSC; k++) {
        m_oscPhase[k] = 0.0f;
        m_oscAmplitude[k] = OSC_BASE_AMP[k];
    }

    m_energySmooth = 0.0f;
    m_energyPeak = 0.0f;
    m_bassSmooth = 0.0f;
    m_trebleSmooth = 0.0f;
    m_beatBurst = 0.0f;
    m_ringPhase = 0.0f;
    m_chromaAngle = 0.0f;
    m_beatFlash = 0.0f;
    m_fallbackPhase = 0.0f;

#if FEATURE_AUDIO_SYNC
    m_lastHopSeq = 0;
    for (uint8_t i = 0; i < 12; i++) m_chromaSmoothed[i] = 0.0f;
#endif
    return true;
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

void LGPFresnelCausticReactiveEffect::render(plugins::EffectContext& ctx) {
    // =====================================================================
    // DELTA TIME
    // =====================================================================
    float rawDt = ctx.getSafeRawDeltaSeconds();
    float dt    = ctx.getSafeDeltaSeconds();

    // =====================================================================
    // AUDIO PROCESSING
    // =====================================================================
    float audioEnergy = 0.0f;
    float bassEnergy  = 0.0f;
    float trebleEnergy = 0.0f;
    uint8_t chromaHueOffset = 0;
    float fadeLevel = 4.0f;
    bool gotBeat = false;
    float beatStr = 0.0f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // ----- Hop-gated chroma update -----
        bool newHop = (ctx.audio.hopSequence() != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.hopSequence();
            for (uint8_t i = 0; i < 12; i++) {
                float target = ctx.audio.getHeavyChroma(i);
                m_chromaSmoothed[i] += (target - m_chromaSmoothed[i]) * 0.25f;
            }
        }

        // Chroma-driven hue
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            m_chromaSmoothed, m_chromaAngle, rawDt, 0.15f);

        // ----- Audio energy (bass-weighted for overall) -----
        float rms  = ctx.audio.rms();
        float bass = ctx.audio.bass();
        audioEnergy = fmaxf(rms, bass * 1.2f);
        if (audioEnergy > 1.0f) audioEnergy = 1.0f;

        // Separate bass and treble energy for oscillator weighting
        bassEnergy = bass;
        if (bassEnergy > 1.0f) bassEnergy = 1.0f;
        trebleEnergy = fmaxf(0.0f, rms - bass * 0.5f) * 2.0f;
        if (trebleEnergy > 1.0f) trebleEnergy = 1.0f;

        // ----- Smooth energy followers -----
        m_energySmooth += (audioEnergy - m_energySmooth) * ENERGY_SLOW_ALPHA;
        float peakAlpha = (audioEnergy > m_energyPeak) ? PEAK_ATTACK : PEAK_RELEASE;
        m_energyPeak += (audioEnergy - m_energyPeak) * peakAlpha;
        if (m_energyPeak > 1.0f) m_energyPeak = 1.0f;

        m_bassSmooth += (bassEnergy - m_bassSmooth) * BASS_SMOOTH_ALPHA;
        m_trebleSmooth += (trebleEnergy - m_trebleSmooth) * TREBLE_SMOOTH_ALPHA;

        // ----- Fade couples to energy squared (more contrast) -----
        fadeLevel = 12.0f + m_energyPeak * m_energyPeak * 28.0f;

        // ----- Beat detection -----
        if (ctx.audio.isOnBeat()) {
            beatStr = ctx.audio.beatStrength();
            gotBeat = true;
            if (beatStr > m_beatFlash) {
                m_beatFlash = beatStr;
            }
            m_beatBurst = fmaxf(m_beatBurst, beatStr);
        }
    } else {
        // No audio: gentle breathing oscillation
        m_fallbackPhase += 0.4f * gSpeedScale * dt;
        if (m_fallbackPhase > kTwoPi * 10.0f) m_fallbackPhase -= kTwoPi * 10.0f;
        m_energySmooth = 0.35f;
        m_energyPeak = sinf(m_fallbackPhase) * 0.3f + 0.3f;
        m_bassSmooth = 0.3f;
        m_trebleSmooth = 0.2f;
        fadeLevel = 15.0f;
        chromaHueOffset = (uint8_t)(m_fallbackPhase * (255.0f / (kTwoPi * 10.0f)));
    }
#else
    m_fallbackPhase += 0.4f * gSpeedScale * dt;
    if (m_fallbackPhase > kTwoPi * 10.0f) m_fallbackPhase -= kTwoPi * 10.0f;
    m_energySmooth = 0.35f;
    m_energyPeak = sinf(m_fallbackPhase) * 0.3f + 0.3f;
    m_bassSmooth = 0.3f;
    m_trebleSmooth = 0.2f;
    fadeLevel = 15.0f;
    chromaHueOffset = (uint8_t)(m_fallbackPhase * (255.0f / (kTwoPi * 10.0f)));
#endif

    // =====================================================================
    // OSCILLATOR BANK -- amplitude modulation + phase accumulation
    // =====================================================================
    for (uint8_t k = 0; k < NUM_OSC; k++) {
        float targetAmp = OSC_BASE_AMP[k]
                        + m_bassSmooth   * OSC_BASS_WEIGHT[k]
                        + m_trebleSmooth * OSC_TREBLE_WEIGHT[k]
                        + m_beatBurst    * OSC_BEAT_BURST[k];
        if (targetAmp > 1.0f) targetAmp = 1.0f;

        // Smooth toward target (EMA)
        m_oscAmplitude[k] += (targetAmp - m_oscAmplitude[k]) * OSC_AMP_SMOOTH[k];

        // Advance phase (frequency scaled by user speed parameter)
        m_oscPhase[k] += OSC_FREQ[k] * gSpeedScale * dt;
        if (m_oscPhase[k] >= kTwoPi) m_oscPhase[k] -= kTwoPi;
    }

    // Decay beat burst
    m_beatBurst = effects::chroma::dtDecay(m_beatBurst, BEAT_BURST_DECAY, rawDt);
    if (m_beatBurst < 0.005f) m_beatBurst = 0.0f;

    // =====================================================================
    // FOCUS POSITION -- weighted sum of all oscillators
    // =====================================================================
    float rawSum = 0.0f;
    float totalAmp = 0.0f;
    for (uint8_t k = 0; k < NUM_OSC; k++) {
        rawSum += m_oscAmplitude[k] * sinf(m_oscPhase[k]);
        totalAmp += m_oscAmplitude[k];
    }

    // Normalise: rawSum / totalAmp maps to [-1, +1]
    float focusRaw;
    if (totalAmp > 0.001f) {
        focusRaw = rawSum / totalAmp;
    } else {
        focusRaw = 0.0f;
    }

    // The 4-oscillator sum rarely reaches ±1 (typical ±0.6).
    // Scale up by 1.6x so the full pixel range is actually used,
    // then clamp to [-1, +1].
    focusRaw *= 1.6f;
    if (focusRaw > 1.0f) focusRaw = 1.0f;
    if (focusRaw < -1.0f) focusRaw = -1.0f;

    // Map to [0, 1] — same as original sweep: (sin * 0.5 + 0.5)
    float focusNorm = focusRaw * 0.5f + 0.5f;

    // BASE range is full strip (like the sweep). Audio WIDENS from a
    // comfortable default — NOT collapses from zero.
    // Quiet = 60% range (48px), loud = 100% range (80px).
    float sweepRange = 0.60f + m_energySmooth * 0.25f + m_energyPeak * 0.15f;
    if (sweepRange > 1.0f) sweepRange = 1.0f;

    // Map to pixel domain
    float focusDist = focusNorm * sweepRange * MAX_D;

    // =====================================================================
    // RING PHASE -- audio-reactive breathing
    // =====================================================================
    float ringSpeed = 0.3f + m_energyPeak * 2.0f + m_beatFlash * 1.5f;
    m_ringPhase += ringSpeed * dt;
    while (m_ringPhase >= kTwoPi) m_ringPhase -= kTwoPi;

    uint8_t ringPhaseU8 = (uint8_t)(m_ringPhase * (255.0f / kTwoPi));

    // =====================================================================
    // BEAT FLASH DECAY
    // =====================================================================
    m_beatFlash = effects::chroma::dtDecay(m_beatFlash, 0.88f, rawDt);
    if (m_beatFlash < 0.01f) m_beatFlash = 0.0f;

    // =====================================================================
    // FADE (audio-coupled persistence)
    // =====================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, (uint8_t)fadeLevel);

    // =====================================================================
    // RENDER LOOP -- Strip 1
    // =====================================================================
    float outputGain255 = gOutputGain * 255.0f;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float d = (float)centerPairDistance(i);

        // Distance from focus
        float x = fabsf(d - focusDist);

        // ----- Core: narrow bright peak -----
        float coreF = 255.0f - x * CORE_SLOPE;
        if (coreF < 0.0f) coreF = 0.0f;
        uint8_t core = (uint8_t)coreF;

        // ----- Sidelobes: Fresnel ring structure -----
        uint8_t xU8 = (x > 255.0f) ? 255 : (uint8_t)x;
        uint8_t ringArg = (uint8_t)(xU8 * RING_SPATIAL_FREQ) + ringPhaseU8;
        uint8_t rings = sin8(ringArg);
        rings = qsub8(rings, RING_SUPPRESS);
        rings = scale8(rings, rings);  // Sharpen peaks

        // ----- Combine core + sidelobes -----
        uint8_t v = qadd8(scale8(core, CORE_GAIN), rings >> 1);

        // ----- Beat flash: wide area around focus (with falloff) -----
        float flashRadius = 3.0f + m_beatFlash * 8.0f;
        if (x <= flashRadius && m_beatFlash > 0.01f) {
            float falloff = 1.0f - (x / flashRadius);
            uint8_t flashBoost = (uint8_t)(m_beatFlash * 200.0f * falloff);
            v = qadd8(v, flashBoost);
        }

        // ----- Centre envelope (matches sweep: floor 80/255=31%) -----
        uint8_t dU8 = (d > MAX_D) ? 255 : (uint8_t)(d * (255.0f / MAX_D));
        uint8_t env = 255 - dU8;
        v = scale8(v, qadd8(80, env >> 1));

        // ----- Colour: chroma-driven, spatial gradient -----
        uint8_t hue = ctx.gHue + chromaHueOffset + (uint8_t)(d * 0.3f);
        uint8_t bright = scale8(v, ctx.brightness);
        if (gOutputGain != 1.0f) {
            bright = scale8(v, (uint8_t)fminf(outputGain255, 255.0f));
            bright = scale8(bright, ctx.brightness);
        }
        ctx.leds[i] = ctx.palette.getColor(hue, bright);

        // ----- Strip 2: parallax depth offset -----
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t ringArg2 = (uint8_t)(xU8 * RING_SPATIAL_FREQ) + ringPhaseU8 + 90;
            uint8_t rings2 = sin8(ringArg2);
            rings2 = qsub8(rings2, RING_SUPPRESS);
            rings2 = scale8(rings2, rings2);

            uint8_t v2 = qadd8(scale8(core, CORE_GAIN), rings2 >> 1);

            if (x <= flashRadius && m_beatFlash > 0.01f) {
                float falloff2 = 1.0f - (x / flashRadius);
                uint8_t flashBoost2 = (uint8_t)(m_beatFlash * 200.0f * falloff2);
                v2 = qadd8(v2, flashBoost2);
            }

            v2 = scale8(v2, qadd8(80, env >> 1));

            uint8_t hue2 = (uint8_t)(hue + 25);
            uint8_t bright2 = scale8(v2, scale8(ctx.brightness, 230));
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue2, bright2);
        }
    }
}

// ---------------------------------------------------------------------------
// cleanup
// ---------------------------------------------------------------------------

void LGPFresnelCausticReactiveEffect::cleanup() {}

// ---------------------------------------------------------------------------
// metadata
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& LGPFresnelCausticReactiveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Fresnel Caustic Reactive",
        "Audio-driven Fresnel caustic with chromatic resonance oscillator bank",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

uint8_t LGPFresnelCausticReactiveEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParams) / sizeof(kParams[0]));
}

const plugins::EffectParameter* LGPFresnelCausticReactiveEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParams[index];
}

bool LGPFresnelCausticReactiveEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "fresnel_reactive_speed_scale") == 0) {
        gSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "fresnel_reactive_output_gain") == 0) {
        gOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "fresnel_reactive_centre_bias") == 0) {
        gCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPFresnelCausticReactiveEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "fresnel_reactive_speed_scale") == 0) return gSpeedScale;
    if (strcmp(name, "fresnel_reactive_output_gain") == 0) return gOutputGain;
    if (strcmp(name, "fresnel_reactive_centre_bias") == 0) return gCentreBias;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
