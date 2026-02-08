/**
 * @file LGPHolographicEsTunedEffect.cpp
 * @brief LGP Holographic (ES tuned) effect implementation
 */

#include "LGPHolographicEsTunedEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos::effects::ieffect {

namespace {

static constexpr float kTwoPi = 6.2831853f;

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline float clamp(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline const float* selectChroma12(const audio::ControlBusFrame& cb) {
    // Prefer ES raw chroma when present (ES backend parity/stability).
    float esSum = 0.0f;
    float lwSum = 0.0f;
    for (uint8_t i = 0; i < audio::CONTROLBUS_NUM_CHROMA; ++i) {
        esSum += cb.es_chroma_raw[i];
        lwSum += cb.chroma[i];
    }
    return (esSum > (lwSum + 0.001f)) ? cb.es_chroma_raw : cb.chroma;
}

static inline uint8_t dominantChromaBin12(const float chroma[audio::CONTROLBUS_NUM_CHROMA]) {
    uint8_t best = 0;
    float bestV = chroma[0];
    for (uint8_t i = 1; i < audio::CONTROLBUS_NUM_CHROMA; ++i) {
        float v = chroma[i];
        if (v > bestV) {
            bestV = v;
            best = i;
        }
    }
    return best;
}

static inline uint8_t chromaBinToHue(uint8_t bin) {
    // 12 bins → 0..252 hue range (21 hue units per semitone).
    return (uint8_t)(bin * 21);
}

static inline float meanAdaptiveBins(const plugins::AudioContext& a, uint8_t start, uint8_t count) {
    float sum = 0.0f;
    for (uint8_t i = 0; i < count; ++i) {
        sum += a.binAdaptive((uint8_t)(start + i));
    }
    return sum / (float)count;
}

} // namespace

bool LGPHolographicEsTunedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0.0f;
    m_phase2 = 0.0f;
    m_phase3 = 0.0f;

    m_lastHopSeq = 0;
    m_dominantChromaBin = 0;
    m_dominantChromaBinSmooth = 0.0f;

    m_lastFastFlux = 0.0f;
    m_refraction = 0.0f;
    m_focus = 0.0f;
    m_lastBeatPhase = 0.0f;

    return true;
}

void LGPHolographicEsTunedEffect::render(plugins::EffectContext& ctx) {
    const float dt = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;
    const float intensityNorm = ctx.brightness / 255.0f;

    // ---------------------------------------------------------------------
    // Audio features (backend-agnostic)
    // ---------------------------------------------------------------------
    float bass = 0.0f;
    float lowMid = 0.0f;
    float treble = 0.0f;
    float flux = 0.0f;
    float beatPhase = 0.0f;
    float beatStrength = 0.0f;
    float tempoConfidence = 0.0f;
    float silentScale = 1.0f;
    bool beatTick = false;
    bool downbeatTick = false;
    bool beatLock = false;

    if (ctx.audio.available) {
        // 64-bin adaptive spectrum is stable across both legacy + ES backend.
        // Zones: low (0..7), mid (16..31), high (48..63)
        bass = meanAdaptiveBins(ctx.audio, 0, 8);
        lowMid = meanAdaptiveBins(ctx.audio, 16, 16);
        treble = meanAdaptiveBins(ctx.audio, 48, 16);

        flux = ctx.audio.fastFlux();
        beatPhase = ctx.audio.beatPhase();
        beatStrength = ctx.audio.beatStrength();
        tempoConfidence = ctx.audio.tempoConfidence();
        silentScale = ctx.audio.controlBus.silentScale;
        beatTick = ctx.audio.isOnBeat();
        downbeatTick = ctx.audio.isOnDownbeat();
        beatLock = (tempoConfidence > 0.45f);

        // Chroma anchor update on hop boundaries.
        if (ctx.audio.controlBus.hop_seq != m_lastHopSeq) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            const float* chroma = selectChroma12(ctx.audio.controlBus);
            m_dominantChromaBin = dominantChromaBin12(chroma);
        }

        float alphaHue = 1.0f - expf(-dt / 0.30f);
        m_dominantChromaBinSmooth += ((float)m_dominantChromaBin - m_dominantChromaBinSmooth) * alphaHue;

        // Flux spike → refraction accent (fast attack, short decay).
        float fluxDelta = flux - m_lastFastFlux;
        m_lastFastFlux = flux;
        if (fluxDelta > 0.22f && flux > 0.25f) {
            m_refraction = 1.0f;
        } else {
            m_refraction *= expf(-dt / 0.18f);
        }

        // Downbeat focus: briefly “snap into focus”, then drift.
        if (downbeatTick) {
            m_focus = 1.0f;
        } else {
            m_focus *= expf(-dt / 0.35f);
        }
    } else {
        // No audio: decay accents slowly.
        m_refraction *= expf(-dt / 0.25f);
        m_focus *= expf(-dt / 0.40f);
        m_dominantChromaBinSmooth *= powf(0.995f, dt * 60.0f);  // dt-corrected
    }

    // ---------------------------------------------------------------------
    // Phase system
    // - Always advances with time (keeps hologram alive)
    // - When beat-locked, phases are gently pulled towards musical ratios
    // ---------------------------------------------------------------------
    const float baseRate = (0.35f + 1.10f * speedNorm);
    m_phase1 += baseRate * 0.55f * kTwoPi * dt;
    m_phase2 += baseRate * 0.85f * kTwoPi * dt;
    m_phase3 += baseRate * 1.25f * kTwoPi * dt;

    // Wrap to prevent unbounded growth
    if (m_phase1 > 1000.0f) m_phase1 = fmodf(m_phase1, kTwoPi);
    if (m_phase2 > 1000.0f) m_phase2 = fmodf(m_phase2, kTwoPi);
    if (m_phase3 > 1000.0f) m_phase3 = fmodf(m_phase3, kTwoPi);

    float beatPhi = beatPhase * kTwoPi;
    if (ctx.audio.available && beatLock) {
        // Target musical ratios (1×, 2×, 4×). Focus reduces detune for a crisp “lock”.
        float focus = clamp01(m_focus);
        float pull = 1.0f - expf(-dt / 0.18f);
        pull *= (0.25f + 0.55f * tempoConfidence);

        float detune = (1.0f - focus) * (0.35f + 0.25f * speedNorm);

        float t1 = beatPhi + detune * 0.15f;
        float t2 = beatPhi * 2.0f + detune * 0.32f;
        float t3 = beatPhi * 4.0f + detune * 0.55f;

        // Soft pull towards targets (keeps it musical but not rigid/robotic).
        m_phase1 += (t1 - m_phase1) * pull;
        m_phase2 += (t2 - m_phase2) * pull;
        m_phase3 += (t3 - m_phase3) * pull;

        // Beat tick can slightly increase refraction to punch on beat without extra triggers.
        if (beatTick && beatStrength > 0.25f) {
            m_refraction = fmaxf(m_refraction, clamp01(beatStrength));
        }
    }

    // Track beat phase for potential future phase-crossing logic (kept for stability parity with other effects).
    m_lastBeatPhase = beatPhase;

    // ---------------------------------------------------------------------
    // Layer gains (instrument voicing)
    // Wide→tight layers respond to bass→treble; shimmer responds to flux/refraction
    // ---------------------------------------------------------------------
    float g1 = 0.55f + 1.05f * bass;
    float g2 = 0.40f + 0.95f * lowMid;
    float g3 = 0.30f + 0.85f * treble;
    float g4 = 0.20f + 1.10f * clamp01(flux * 0.8f + m_refraction);

    // Silence gating: the hologram should breathe down cleanly.
    g1 *= silentScale;
    g2 *= silentScale;
    g3 *= silentScale;
    g4 *= silentScale;

    // Prevent hard zeros (keeps a base hologram even in quieter passages).
    g1 = clamp(g1, 0.10f, 1.60f);
    g2 = clamp(g2, 0.08f, 1.45f);
    g3 = clamp(g3, 0.06f, 1.35f);
    g4 = clamp(g4, 0.04f, 1.30f);

    // Contrast drive: more energy/beat → crisper interference.
    float energy = clamp01(0.35f * bass + 0.25f * lowMid + 0.20f * treble + 0.20f * beatStrength);
    float drive = 1.0f + 1.55f * energy + 0.55f * clamp01(m_focus);

    // ---------------------------------------------------------------------
    // Colour anchoring (non-rainbow): chroma bin sets the base, palette does the rest.
    // ---------------------------------------------------------------------
    uint8_t baseHue = chromaBinToHue((uint8_t)(m_dominantChromaBinSmooth + 0.5f));
    uint8_t dispersion = (uint8_t)(96 + (uint8_t)(m_refraction * 28.0f));

    // ---------------------------------------------------------------------
    // Render
    // ---------------------------------------------------------------------
    const int numLayers = 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);

        float layerSum = 0.0f;

        // Spatial frequencies are the same "holographic" family as the base effect.
        // Audio only modulates gains and time bases.
        layerSum += sinf(dist * 0.05f + m_phase1) * g1;
        layerSum += sinf(dist * 0.15f + m_phase2) * (0.7f * g2);
        layerSum += sinf(dist * 0.30f + m_phase3) * (0.5f * g3);
        layerSum += sinf(dist * 0.60f - m_phase1 * 3.0f) * (0.3f * g4);

        layerSum = layerSum / (float)numLayers;
        layerSum = tanhf(layerSum * drive);

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * layerSum * intensityNorm);

        // “Refraction” is palette shear; not a hue sweep.
        float shear = 10.0f + 18.0f * m_refraction + 10.0f * clamp01(m_focus);
        uint8_t paletteIndex1 = (uint8_t)(baseHue + (uint8_t)(dist * 0.60f) + (int8_t)(layerSum * shear));
        uint8_t paletteIndex2 = (uint8_t)(baseHue + dispersion - (uint8_t)(dist * 0.60f) - (int8_t)(layerSum * shear));

        ctx.leds[i] = ctx.palette.getColor(paletteIndex1, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(paletteIndex2, brightness);
        }
    }
}

void LGPHolographicEsTunedEffect::cleanup() {}

const plugins::EffectMetadata& LGPHolographicEsTunedEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Holographic (ES tuned)",
        "Musically driven holographic interference (beat/flux/chroma)",
        plugins::EffectCategory::QUANTUM,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect
