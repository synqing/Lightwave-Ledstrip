/**
 * @file LGPBeatPulseEffect.cpp
 * @brief Beat-synchronized radial pulse implementation
 */

#include "LGPBeatPulseEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

static inline const float* selectChroma12(const audio::ControlBusFrame& cb) {
    // Both backends now produce normalised chroma via Stage A/B pipeline.
    return cb.chroma;
}

static inline uint8_t dominantChromaBin12(const float chroma[audio::CONTROLBUS_NUM_CHROMA], float* outMax = nullptr) {
    uint8_t best = 0;
    float bestV = chroma[0];
    for (uint8_t i = 1; i < audio::CONTROLBUS_NUM_CHROMA; ++i) {
        float v = chroma[i];
        if (v > bestV) {
            bestV = v;
            best = i;
        }
    }
    if (outMax) *outMax = bestV;
    return best;
}

static inline uint8_t chromaBinToHue(uint8_t bin) {
    // 12 bins → 0..252 hue range (21 hue units per semitone).
    return (uint8_t)(bin * 21);
}

} // namespace

bool LGPBeatPulseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // Primary kick/beat pulse
    m_pulsePosition = 0.0f;
    m_pulseIntensity = 0.0f;
    m_fallbackPhase = 0.0f;
    m_lastBeatPhase = 0.0f;

    // Snare detection and secondary pulse
    m_lastMidEnergy = 0.0f;
    m_snarePulsePos = 0.0f;
    m_snarePulseInt = 0.0f;

    // Hi-hat detection and shimmer
    m_lastTrebleEnergy = 0.0f;
    m_hihatShimmer = 0.0f;

    m_lastFastFlux = 0.0f;
    m_lastHopSeq = 0;
    m_dominantChromaBin = 0;
    m_dominantChromaBinSmooth = 0.0f;
    m_bandsLowFrames = 0;
    m_smoothBrightness = ctx.brightness / 255.0f;
    m_smoothStrength = 0.3f;
    m_smoothBeatPhase = 0.0f;

    return true;
}

void LGPBeatPulseEffect::render(plugins::EffectContext& ctx) {
    float beatPhase;
    float bassEnergy;
    float midEnergy = 0.0f;
    float trebleEnergy = 0.0f;
    bool onBeat = false;
    bool snareHit = false;
    bool hihatHit = false;

    // Spike detection thresholds (tuned for typical band levels ~0.05–0.3)
    constexpr float SNARE_SPIKE_THRESH = 0.18f;   // Mid energy spike threshold
    constexpr float HIHAT_SPIKE_THRESH = 0.14f;   // Treble energy spike threshold
    constexpr float FLUX_SPIKE_THRESH = 0.18f;    // Flux spike threshold (backend-agnostic onset proxy)

    if (ctx.audio.available) {
        beatPhase = ctx.audio.beatPhase();
        bassEnergy = ctx.audio.bass();
        midEnergy = ctx.audio.mid();
        trebleEnergy = ctx.audio.treble();
        onBeat = ctx.audio.isOnBeat();

        // Bands-dead guard: if bands stay near zero for N frames, don't trust band-based triggers
        float bandSum = bassEnergy + midEnergy + trebleEnergy;
        if (bandSum < BANDS_LOW_THRESHOLD) {
            if (m_bandsLowFrames < BANDS_LOW_FRAMES_MAX) ++m_bandsLowFrames;
        } else {
            m_bandsLowFrames = 0;
        }
        bool bandsTrusted = (m_bandsLowFrames < BANDS_LOW_FRAMES_MAX);

        // Update chroma anchor on hop boundaries (keeps colour stable between hops).
        if (ctx.audio.controlBus.hop_seq != m_lastHopSeq) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            const float* chroma = selectChroma12(ctx.audio.controlBus);
            m_dominantChromaBin = dominantChromaBin12(chroma);
        }

        // Smooth chroma bin (prevents rapid hue jitter on sparse chroma).
        float rawDt = ctx.getSafeRawDeltaSeconds();
        float alpha = 1.0f - expf(-rawDt / 0.20f);
        m_dominantChromaBinSmooth += ((float)m_dominantChromaBin - m_dominantChromaBinSmooth) * alpha;

        // Snare detection: spike in mid-frequency energy (only when bands look live)
        if (bandsTrusted) {
            float midDelta = midEnergy - m_lastMidEnergy;
            if (midDelta > SNARE_SPIKE_THRESH && midEnergy > 0.15f) {
                snareHit = true;
            }

            // Hi-hat detection: spike in high-frequency energy
            float trebleDelta = trebleEnergy - m_lastTrebleEnergy;
            if (trebleDelta > HIHAT_SPIKE_THRESH && trebleEnergy > 0.12f) {
                hihatHit = true;
            }
        }

        // Flux accent: a reliable onset proxy for backends without explicit snare triggers.
        float flux = ctx.audio.fastFlux();
        float fluxDelta = flux - m_lastFastFlux;
        m_lastFastFlux = flux;
        if (fluxDelta > FLUX_SPIKE_THRESH && flux > 0.18f) {
            // Treat as snare-like transient when there is some mid/treble content.
            if (midEnergy > 0.12f || trebleEnergy > 0.10f) {
                snareHit = true;
            } else {
                hihatHit = true;
            }
        }

        // Store for next frame
        m_lastMidEnergy = midEnergy;
        m_lastTrebleEnergy = trebleEnergy;
    } else {
        // Fallback: 120 BPM with simulated snare on off-beats
        m_fallbackPhase += ((ctx.deltaTimeSeconds * 1000.0f) / 500.0f);
        if (m_fallbackPhase >= 1.0f) m_fallbackPhase -= 1.0f;
        beatPhase = m_fallbackPhase;
        bassEnergy = 0.5f;

        // Detect beat crossing (kick)
        if (beatPhase < 0.05f && m_lastBeatPhase > 0.95f) {
            onBeat = true;
        }
        // Simulate snare on off-beat (around 0.5 phase)
        if (beatPhase > 0.48f && beatPhase < 0.52f && m_lastBeatPhase < 0.48f) {
            snareHit = true;
        }
        // Simulate hi-hat every eighth note
        float hihatPhase = fmodf(beatPhase * 4.0f, 1.0f);
        float lastHihatPhase = fmodf(m_lastBeatPhase * 4.0f, 1.0f);
        if (hihatPhase < 0.1f && lastHihatPhase > 0.9f) {
            hihatHit = true;
        }
    }
    m_lastBeatPhase = beatPhase;

    // Smooth context so audio mapping / auto-speed don't cause flashing or cut-off
    float brightnessNorm = ctx.brightness / 255.0f;
    float strength = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float blend = 0.08f;
    m_smoothBrightness += (brightnessNorm - m_smoothBrightness) * blend;
    m_smoothStrength += (strength - m_smoothStrength) * blend;
    m_smoothBeatPhase += (beatPhase - m_smoothBeatPhase) * blend;

    // === PRIMARY PULSE (Kick/Beat) ===
    if (onBeat) {
        m_pulsePosition = 0.0f;
        // silentScale handled globally by RendererActor
        float base = 0.25f + 0.75f * fminf(1.0f, bassEnergy * 1.15f + m_smoothStrength * 0.65f);
        m_pulseIntensity = base;
    }

    // Expand pulse outward (~400ms to reach edge)
    m_pulsePosition += (ctx.deltaTimeSeconds * 1000.0f) / 400.0f;
    if (m_pulsePosition > 1.0f) m_pulsePosition = 1.0f;

    // Decay intensity (dt-corrected for frame-rate independence)
    float rawDt2 = ctx.getSafeRawDeltaSeconds();
    m_pulseIntensity *= powf(0.95f, rawDt2 * 60.0f);
    if (m_pulseIntensity < 0.01f) m_pulseIntensity = 0.0f;

    // === SECONDARY PULSE (Snare) ===
    if (snareHit) {
        m_snarePulsePos = 0.0f;
        m_snarePulseInt = 0.6f + midEnergy * 0.4f;
    }

    // Snare pulse expands faster (~300ms)
    m_snarePulsePos += (ctx.deltaTimeSeconds * 1000.0f) / 300.0f;
    if (m_snarePulsePos > 1.0f) m_snarePulsePos = 1.0f;

    // Faster decay for snare (dt-corrected)
    m_snarePulseInt *= powf(0.92f, rawDt2 * 60.0f);
    if (m_snarePulseInt < 0.01f) m_snarePulseInt = 0.0f;

    // === SHIMMER OVERLAY (Hi-hat) ===
    if (hihatHit) {
        m_hihatShimmer = 0.8f + trebleEnergy * 0.2f;
    }

    // Very fast decay for hi-hat sparkle (dt-corrected)
    m_hihatShimmer *= powf(0.88f, rawDt2 * 60.0f);
    if (m_hihatShimmer < 0.01f) m_hihatShimmer = 0.0f;

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // === RENDER CENTER PAIR OUTWARD ===
    uint8_t baseHue = chromaBinToHue((uint8_t)(m_dominantChromaBinSmooth + 0.5f));
    // Fixed offsets (no time-based hue cycling): keeps colour musically anchored (non-rainbow).
    uint8_t primaryHue = baseHue;
    uint8_t snareHue = (uint8_t)(baseHue + 42);  // ~perfect fifth-ish shift
    uint8_t hihatHue = (uint8_t)(baseHue + 96);  // brighter offset

    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        float normalizedDist = (float)dist / HALF_LENGTH;

        // --- Primary pulse ring (kick) ---
        float ringDist = fabsf(normalizedDist - m_pulsePosition);
        float ringWidth = 0.15f;
        float primaryBright = 0.0f;

        if (ringDist < ringWidth) {
            primaryBright = (1.0f - ringDist / ringWidth) * m_pulseIntensity;
        }

        // --- Secondary pulse ring (snare) - thinner, faster ---
        float snareRingDist = fabsf(normalizedDist - m_snarePulsePos);
        float snareRingWidth = 0.08f;  // Thinner than primary
        float snareBright = 0.0f;

        if (snareRingDist < snareRingWidth) {
            snareBright = (1.0f - snareRingDist / snareRingWidth) * m_snarePulseInt;
        }

        // --- Hi-hat shimmer overlay ---
        // Sparkle pattern: pseudo-random based on position and time
        float shimmerBright = 0.0f;
        if (m_hihatShimmer > 0.05f) {
            // Create sparkle pattern using position hash
            uint8_t sparkleHash = (uint8_t)((dist * 73 + (int)(beatPhase * 256)) & 0xFF);
            if ((sparkleHash & 0x0F) < 3) {  // ~20% of LEDs sparkle
                shimmerBright = m_hihatShimmer * ((sparkleHash >> 4) / 16.0f);
            }
        }

        // Background glow: use smoothed phase/strength so it holds and doesn't flash
        float bgBright = 0.08f + 0.08f * m_smoothBeatPhase + 0.08f * m_smoothStrength;
        if (bgBright < 0.10f) bgBright = 0.10f;  // Floor so background always visible, no "random flashes"

        // --- Combine all layers ---
        // Calculate combined color (normalised by layer count to prevent wash to white)
        CRGB finalColor = CRGB::Black;
        uint8_t layerCount = 1;  // base always present

        // Layer 1: Background glow (use smoothed brightness so display holds)
        uint8_t bgBrightness = (uint8_t)(bgBright * m_smoothBrightness * 255.0f);
        if (bgBrightness < 20u) bgBrightness = 20u;
        finalColor = ctx.palette.getColor(primaryHue, bgBrightness);

        // Layer 2: Primary pulse (smoothed brightness)
        if (primaryBright > 0.01f) {
            uint8_t pBright = (uint8_t)(primaryBright * m_smoothBrightness * 255.0f);
            CRGB primaryColor = ctx.palette.getColor(primaryHue, pBright);
            finalColor.r = (finalColor.r + primaryColor.r > 255) ? 255 : (uint8_t)(finalColor.r + primaryColor.r);
            finalColor.g = (finalColor.g + primaryColor.g > 255) ? 255 : (uint8_t)(finalColor.g + primaryColor.g);
            finalColor.b = (finalColor.b + primaryColor.b > 255) ? 255 : (uint8_t)(finalColor.b + primaryColor.b);
            layerCount++;
        }

        // Layer 3: Snare pulse (complementary color, smoothed brightness)
        if (snareBright > 0.01f) {
            uint8_t sBright = (uint8_t)(snareBright * m_smoothBrightness * 255.0f * 0.7f);  // Slightly dimmer
            CRGB snareColor = ctx.palette.getColor(snareHue, sBright);
            finalColor.r = (finalColor.r + snareColor.r > 255) ? 255 : (uint8_t)(finalColor.r + snareColor.r);
            finalColor.g = (finalColor.g + snareColor.g > 255) ? 255 : (uint8_t)(finalColor.g + snareColor.g);
            finalColor.b = (finalColor.b + snareColor.b > 255) ? 255 : (uint8_t)(finalColor.b + snareColor.b);
            layerCount++;
        }

        // Layer 4: Hi-hat shimmer (smoothed brightness)
        if (shimmerBright > 0.01f) {
            uint8_t hBright = (uint8_t)(shimmerBright * m_smoothBrightness * 255.0f * 0.5f);  // Subtle
            CRGB hihatColor = ctx.palette.getColor(hihatHue, hBright);
            finalColor.r = (finalColor.r + hihatColor.r > 255) ? 255 : (uint8_t)(finalColor.r + hihatColor.r);
            finalColor.g = (finalColor.g + hihatColor.g > 255) ? 255 : (uint8_t)(finalColor.g + hihatColor.g);
            finalColor.b = (finalColor.b + hihatColor.b > 255) ? 255 : (uint8_t)(finalColor.b + hihatColor.b);
            layerCount++;
        }

        // Normalise by layer count so combined colour stays in range and preserves hue (colour corruption fix)
        if (layerCount > 1) {
            finalColor.nscale8(255u / (unsigned)layerCount);
        }

        SET_CENTER_PAIR(ctx, dist, finalColor);
    }
}

void LGPBeatPulseEffect::cleanup() {}

const plugins::EffectMetadata& LGPBeatPulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse",
        "Radial pulse with snare/hihat response",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
