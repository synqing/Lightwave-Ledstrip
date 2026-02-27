/**
 * @file LGPFresnelCausticSweepEffect.cpp
 * @brief LGP Fresnel Caustic Focus Sweep -- scanning lens caustic with Fresnel sidelobes
 *
 * A sinusoidally sweeping focus point in distance-from-centre space with:
 * - Narrow Gaussian-like core at the focus (~3 LEDs)
 * - Oscillatory Fresnel ring sidelobes that breathe slowly
 * - Centre-weighted envelope (brightest action near strip midpoint)
 * - Specular flash on beat at current focus position
 */

#include "LGPFresnelCausticSweepEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPFresnelCausticSweepEffect
namespace {
constexpr float kLGPFresnelCausticSweepEffectSpeedScale = 1.0f;
constexpr float kLGPFresnelCausticSweepEffectOutputGain = 1.0f;
constexpr float kLGPFresnelCausticSweepEffectCentreBias = 1.0f;

float gLGPFresnelCausticSweepEffectSpeedScale = kLGPFresnelCausticSweepEffectSpeedScale;
float gLGPFresnelCausticSweepEffectOutputGain = kLGPFresnelCausticSweepEffectOutputGain;
float gLGPFresnelCausticSweepEffectCentreBias = kLGPFresnelCausticSweepEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPFresnelCausticSweepEffectParameters[] = {
    {"lgpfresnel_caustic_sweep_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPFresnelCausticSweepEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpfresnel_caustic_sweep_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPFresnelCausticSweepEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpfresnel_caustic_sweep_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPFresnelCausticSweepEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPFresnelCausticSweepEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr float kTwoPi = 6.2831853f;

// Full sweep period at speed=25 (normalised speed 0.5): ~8 seconds
// focusPhase advances at speedNorm * BASE_SWEEP_RATE rad/s
// At speedNorm=0.5 => 0.5 * 1.571 ~= 0.785 rad/s => ~8s full cycle
static constexpr float BASE_SWEEP_RATE = 1.5707963f;   // PI/2 rad/s

// Sidelobe breathing rate -- slow independent drift
static constexpr float RING_BREATHE_RATE = 0.3f;       // rad/s

// Maximum distance from centre (half strip length)
static constexpr float MAX_D = 80.0f;

// Core Gaussian width control: brightness = max(0, 255 - x * CORE_SLOPE)
// At CORE_SLOPE=85 the core is ~3 LEDs wide before it drops to zero
static constexpr float CORE_SLOPE = 85.0f;

// Sidelobe spatial frequency (higher = tighter rings)
static constexpr uint8_t RING_SPATIAL_FREQ = 18;

// Sidelobe suppression threshold (removes dim oscillation tails)
static constexpr uint8_t RING_SUPPRESS = 90;

// Core vs sidelobe mixing: core scaled by 200/255, rings halved
static constexpr uint8_t CORE_GAIN = 200;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

LGPFresnelCausticSweepEffect::LGPFresnelCausticSweepEffect()
    : m_focusPhase(0.0f)
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

bool LGPFresnelCausticSweepEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPFresnelCausticSweepEffect
    gLGPFresnelCausticSweepEffectSpeedScale = kLGPFresnelCausticSweepEffectSpeedScale;
    gLGPFresnelCausticSweepEffectOutputGain = kLGPFresnelCausticSweepEffectOutputGain;
    gLGPFresnelCausticSweepEffectCentreBias = kLGPFresnelCausticSweepEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPFresnelCausticSweepEffect

    m_focusPhase = 0.0f;
    m_ringPhase = 0.0f;
    m_chromaAngle = 0.0f;
    m_beatFlash = 0.0f;
    m_fallbackPhase = 0.0f;
#if FEATURE_AUDIO_SYNC
    m_lastHopSeq = 0;
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaSmoothed[i] = 0.0f;
    }
#endif
    return true;
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

void LGPFresnelCausticSweepEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // SAFE DELTA TIME
    // =========================================================================
    float rawDt = ctx.getSafeRawDeltaSeconds();
    float dt    = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;

    // =========================================================================
    // AUDIO PROCESSING
    // =========================================================================
    float sweepSpeedMult = 1.0f;
    uint8_t chromaHueOffset = 0;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // ----- Hop-gated chroma update -----
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            for (uint8_t i = 0; i < 12; i++) {
                // Gentle exponential approach toward target (per-hop)
                float target = ctx.audio.controlBus.heavy_chroma[i];
                m_chromaSmoothed[i] += (target - m_chromaSmoothed[i]) * 0.25f;
            }
        }

        // Circular weighted mean + circular EMA for smooth, continuous hue
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            m_chromaSmoothed, m_chromaAngle, rawDt, 0.20f);

        // RMS modulates sweep speed: +-30% around nominal
        float rms = ctx.audio.rms();
        sweepSpeedMult = 1.0f + (rms - 0.3f) * 1.0f;  // 0.7 .. 1.7 range
        if (sweepSpeedMult < 0.7f) sweepSpeedMult = 0.7f;
        if (sweepSpeedMult > 1.7f) sweepSpeedMult = 1.7f;

        // Beat triggers specular flash
        if (ctx.audio.isOnBeat()) {
            float str = ctx.audio.beatStrength();
            if (str > m_beatFlash) {
                m_beatFlash = str;
            }
        }
    } else {
        // No audio: slow fallback oscillation
        m_fallbackPhase += speedNorm * 0.4f * dt;
        if (m_fallbackPhase > kTwoPi * 10.0f) {
            m_fallbackPhase -= kTwoPi * 10.0f;
        }
    }
#else
    // No audio feature: slow fallback oscillation
    m_fallbackPhase += speedNorm * 0.4f * dt;
    if (m_fallbackPhase > kTwoPi * 10.0f) {
        m_fallbackPhase -= kTwoPi * 10.0f;
    }
#endif

    // =========================================================================
    // PHASE ACCUMULATION
    // =========================================================================
    m_focusPhase += speedNorm * BASE_SWEEP_RATE * sweepSpeedMult * dt;
    while (m_focusPhase >= kTwoPi) m_focusPhase -= kTwoPi;
    while (m_focusPhase < 0.0f)   m_focusPhase += kTwoPi;

    m_ringPhase += RING_BREATHE_RATE * dt;
    while (m_ringPhase >= kTwoPi) m_ringPhase -= kTwoPi;

    // Decay beat flash (dt-corrected: ~0.88 per frame at 60fps => fast decay)
    m_beatFlash = effects::chroma::dtDecay(m_beatFlash, 0.88f, rawDt);
    if (m_beatFlash < 0.01f) m_beatFlash = 0.0f;

    // =========================================================================
    // COMPUTE FOCUS POSITION
    // =========================================================================
    // Sinusoidal sweep across distance-from-centre domain [0..MAX_D]
    float focusPos = (sinf(m_focusPhase) * 0.5f + 0.5f) * MAX_D;

    // Ring phase offset (8-bit domain) for sidelobe breathing
    uint8_t ringPhaseU8 = (uint8_t)(m_ringPhase * (255.0f / kTwoPi));

    // =========================================================================
    // FADE (persistence trails)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // =========================================================================
    // RENDER LOOP -- Strip 1 (i = 0..159)
    // =========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float d = (float)centerPairDistance(i);

        // Distance from the current focus point
        float x = fabsf(d - focusPos);

        // ----- Core: narrow bright peak -----
        // Linear falloff clamped at zero: drops to 0 within ~3 LEDs
        float coreF = 255.0f - x * CORE_SLOPE;
        if (coreF < 0.0f) coreF = 0.0f;
        uint8_t core = (uint8_t)coreF;

        // ----- Sidelobes: Fresnel ring structure -----
        // Oscillatory falloff using sin8 with distance from focus
        uint8_t xU8 = (x > 255.0f) ? 255 : (uint8_t)x;
        uint8_t ringArg = (uint8_t)(xU8 * RING_SPATIAL_FREQ) + ringPhaseU8;
        uint8_t rings = sin8(ringArg);
        rings = qsub8(rings, RING_SUPPRESS);    // suppress low values
        rings = scale8(rings, rings);            // sharpen peaks (square)

        // ----- Combine core + sidelobes -----
        uint8_t v = qadd8(scale8(core, CORE_GAIN), rings >> 1);

        // ----- Centre envelope -----
        // So the effect reads as "inside the acrylic" not "on the edge"
        uint8_t dU8 = (d > MAX_D) ? 255 : (uint8_t)(d * (255.0f / MAX_D));
        uint8_t env = 255 - dU8;
        v = scale8(v, qadd8(80, env >> 1));      // floor of ~80/255 + envelope

        // ----- Specular highlight at exact focus -----
        if (x <= 1.5f && v > 180) {
            // Beat flash boosts toward white
            uint8_t flashBoost = (uint8_t)(m_beatFlash * 75.0f);
            v = qadd8(v, flashBoost);
        }

        // ----- Colour via palette -----
        uint8_t hue = ctx.gHue + chromaHueOffset + (uint8_t)(d * 0.3f);
        uint8_t bright = scale8(v, ctx.brightness);
        ctx.leds[i] = ctx.palette.getColor(hue, bright);

        // ----- Strip 2: parallax depth offset -----
        if (i + STRIP_LENGTH < ctx.ledCount) {
            // +90 on ring phase for different ring alignment (parallax)
            uint8_t ringArg2 = (uint8_t)(xU8 * RING_SPATIAL_FREQ) + ringPhaseU8 + 90;
            uint8_t rings2 = sin8(ringArg2);
            rings2 = qsub8(rings2, RING_SUPPRESS);
            rings2 = scale8(rings2, rings2);

            uint8_t v2 = qadd8(scale8(core, CORE_GAIN), rings2 >> 1);
            v2 = scale8(v2, qadd8(80, env >> 1));

            if (x <= 1.5f && v2 > 180) {
                uint8_t flashBoost2 = (uint8_t)(m_beatFlash * 75.0f);
                v2 = qadd8(v2, flashBoost2);
            }

            // Hue +25 for strip 2, brightness 90%
            uint8_t hue2 = (uint8_t)(hue + 25);
            uint8_t bright2 = scale8(v2, scale8(ctx.brightness, 230));  // ~90%
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue2, bright2);
        }
    }
}

// ---------------------------------------------------------------------------
// cleanup
// ---------------------------------------------------------------------------


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPFresnelCausticSweepEffect
uint8_t LGPFresnelCausticSweepEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPFresnelCausticSweepEffectParameters) / sizeof(kLGPFresnelCausticSweepEffectParameters[0]));
}

const plugins::EffectParameter* LGPFresnelCausticSweepEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPFresnelCausticSweepEffectParameters[index];
}

bool LGPFresnelCausticSweepEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpfresnel_caustic_sweep_effect_speed_scale") == 0) {
        gLGPFresnelCausticSweepEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpfresnel_caustic_sweep_effect_output_gain") == 0) {
        gLGPFresnelCausticSweepEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpfresnel_caustic_sweep_effect_centre_bias") == 0) {
        gLGPFresnelCausticSweepEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPFresnelCausticSweepEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpfresnel_caustic_sweep_effect_speed_scale") == 0) return gLGPFresnelCausticSweepEffectSpeedScale;
    if (strcmp(name, "lgpfresnel_caustic_sweep_effect_output_gain") == 0) return gLGPFresnelCausticSweepEffectOutputGain;
    if (strcmp(name, "lgpfresnel_caustic_sweep_effect_centre_bias") == 0) return gLGPFresnelCausticSweepEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPFresnelCausticSweepEffect

void LGPFresnelCausticSweepEffect::cleanup() {
    // No resources to free
}

// ---------------------------------------------------------------------------
// getMetadata
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& LGPFresnelCausticSweepEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Fresnel Caustic Sweep",
        "Scanning lens caustic with Fresnel sidelobe ring structure",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
