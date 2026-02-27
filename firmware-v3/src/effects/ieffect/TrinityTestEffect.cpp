#include "TrinityTestEffect.h"
#include "../CoreEffects.h"
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool TrinityTestEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatFlashDecay = 0.0f;
    m_lastBeatPhase = 0.0f;
    m_lastDataTime = 0;
    m_frameCount = 0;
    m_smoothEnergy = 0.0f;
    m_smoothBass = 0.0f;
    m_smoothTreble = 0.0f;
    m_smoothPerc = 0.0f;
    m_smoothVocal = 0.0f;
    return true;
}

float TrinityTestEffect::smooth(float current, float target, float dt, float speed) {
    float alpha = 1.0f - expf(-speed * dt);
    return current + (target - current) * alpha;
}

void TrinityTestEffect::setSymmetricLEDs(plugins::EffectContext& ctx, uint8_t distFromCenter, CRGB color) {
    // CENTER ORIGIN pattern: set all 4 LEDs at this distance from center
    // Strip 1 left: 79 - dist, Strip 1 right: 80 + dist
    // Strip 2 left: 239 - dist, Strip 2 right: 240 + dist

    if (distFromCenter >= 80) return;  // Out of bounds

    uint16_t s1Left = 79 - distFromCenter;
    uint16_t s1Right = 80 + distFromCenter;
    uint16_t s2Left = 239 - distFromCenter;
    uint16_t s2Right = 240 + distFromCenter;

    if (s1Left < ctx.ledCount) ctx.leds[s1Left] = color;
    if (s1Right < ctx.ledCount) ctx.leds[s1Right] = color;
    if (s2Left < ctx.ledCount) ctx.leds[s2Left] = color;
    if (s2Right < ctx.ledCount) ctx.leds[s2Right] = color;
}

void TrinityTestEffect::renderNoDataWarning(plugins::EffectContext& ctx) {
    // Pulsing dim red to indicate NO AUDIO DATA
    m_frameCount++;
    float pulse = sinf(m_frameCount * 0.05f) * 0.3f + 0.4f;  // 0.1 to 0.7 brightness
    uint8_t brightness = (uint8_t)(pulse * ctx.brightness * 0.5f);

    CRGB warningColor = CRGB(brightness, 0, 0);  // Red

    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        ctx.leds[i] = warningColor;
    }

    // Center pair shows brighter to indicate "waiting for data"
    CRGB centerColor = CRGB(ctx.brightness, ctx.brightness / 4, 0);  // Orange
    setSymmetricLEDs(ctx, 0, centerColor);
    setSymmetricLEDs(ctx, 1, centerColor);
}

void TrinityTestEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeRawDeltaSeconds();
    m_frameCount++;

    // Clear all LEDs first
    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        ctx.leds[i] = CRGB::Black;
    }

#if FEATURE_AUDIO_SYNC
    // TRINITY-ONLY: Only visualize PRISM data, ignore microphone fallback
    // This makes the effect a true diagnostic for Trinity data flow
    if (!ctx.audio.available || !ctx.audio.trinityActive) {
        renderNoDataWarning(ctx);
        return;
    }

    // Record that we received data
    m_lastDataTime = ctx.totalTimeMs;

    // =========================================================================
    // EXTRACT TRINITY DATA
    // =========================================================================

    float beatPhase = ctx.audio.beatPhase();      // 0.0 - 1.0, sweeps each beat
    bool onBeat = ctx.audio.isOnBeat();           // True momentarily on beat
    float rms = ctx.audio.rms();                  // Energy (0-1)
    float bass = ctx.audio.bass();                // Bass weight (0-1)
    float treble = ctx.audio.treble();            // Brightness/treble (0-1)
    float flux = ctx.audio.flux();                // Percussiveness (0-1)

    // Smooth the values for stable visualization
    m_smoothEnergy = smooth(m_smoothEnergy, rms, dt, 12.0f);
    m_smoothBass = smooth(m_smoothBass, bass, dt, 10.0f);
    m_smoothTreble = smooth(m_smoothTreble, treble, dt, 10.0f);
    m_smoothPerc = smooth(m_smoothPerc, flux, dt, 15.0f);

    // =========================================================================
    // BEAT FLASH (all LEDs flash white on beat)
    // =========================================================================

    if (onBeat) {
        m_beatFlashDecay = 1.0f;  // Full flash
    }
    m_beatFlashDecay = smooth(m_beatFlashDecay, 0.0f, dt, 8.0f);  // Decay quickly

    // Apply beat flash as additive white overlay
    if (m_beatFlashDecay > 0.01f) {
        uint8_t flashBrightness = (uint8_t)(m_beatFlashDecay * ctx.brightness * 0.5f);
        CRGB flashColor = CRGB(flashBrightness, flashBrightness, flashBrightness);
        for (uint16_t i = 0; i < ctx.ledCount; i++) {
            ctx.leds[i] += flashColor;
        }
    }

    // =========================================================================
    // ZONE 1: BEAT PHASE RING (0-19 from center) - White sweeping indicator
    // =========================================================================

    // The beat phase sweep indicator - shows current position in beat
    uint8_t phasePos = (uint8_t)(beatPhase * ZONE_PHASE_END);

    for (uint8_t d = 0; d <= ZONE_PHASE_END; d++) {
        uint8_t brightness = 20;  // Dim background

        // Highlight the current phase position
        if (d == phasePos) {
            brightness = ctx.brightness;  // Full brightness at phase position
        } else if (d == (phasePos > 0 ? phasePos - 1 : ZONE_PHASE_END)) {
            brightness = ctx.brightness / 2;  // Trail
        }

        setSymmetricLEDs(ctx, d, CRGB(brightness, brightness, brightness));  // White
    }

    // =========================================================================
    // ZONE 2: ENERGY LEVEL (20-39 from center) - Yellow/Orange bar
    // =========================================================================

    uint8_t energyLevel = (uint8_t)(m_smoothEnergy * (ZONE_ENERGY_END - ZONE_PHASE_END));

    for (uint8_t d = ZONE_PHASE_END + 1; d <= ZONE_ENERGY_END; d++) {
        uint8_t zoneDist = d - ZONE_PHASE_END - 1;

        if (zoneDist <= energyLevel) {
            // Gradient from yellow to red based on level
            uint8_t red = ctx.brightness;
            uint8_t green = (uint8_t)(ctx.brightness * (1.0f - (float)zoneDist / (ZONE_ENERGY_END - ZONE_PHASE_END)));
            setSymmetricLEDs(ctx, d, CRGB(red, green, 0));
        } else {
            // Dim background
            setSymmetricLEDs(ctx, d, CRGB(15, 10, 0));
        }
    }

    // =========================================================================
    // ZONE 3: BASS LEVEL (40-54 from center) - Red bar
    // =========================================================================

    uint8_t bassLevel = (uint8_t)(m_smoothBass * (ZONE_BASS_END - ZONE_ENERGY_END));

    for (uint8_t d = ZONE_ENERGY_END + 1; d <= ZONE_BASS_END; d++) {
        uint8_t zoneDist = d - ZONE_ENERGY_END - 1;

        if (zoneDist <= bassLevel) {
            uint8_t intensity = ctx.brightness;
            setSymmetricLEDs(ctx, d, CRGB(intensity, 0, intensity / 4));  // Red-magenta
        } else {
            setSymmetricLEDs(ctx, d, CRGB(15, 0, 5));  // Dim background
        }
    }

    // =========================================================================
    // ZONE 4: TREBLE/BRIGHTNESS LEVEL (55-69 from center) - Blue/Cyan bar
    // =========================================================================

    uint8_t trebleLevel = (uint8_t)(m_smoothTreble * (ZONE_TREBLE_END - ZONE_BASS_END));

    for (uint8_t d = ZONE_BASS_END + 1; d <= ZONE_TREBLE_END; d++) {
        uint8_t zoneDist = d - ZONE_BASS_END - 1;

        if (zoneDist <= trebleLevel) {
            uint8_t intensity = ctx.brightness;
            setSymmetricLEDs(ctx, d, CRGB(0, intensity / 2, intensity));  // Cyan-blue
        } else {
            setSymmetricLEDs(ctx, d, CRGB(0, 5, 15));  // Dim background
        }
    }

    // =========================================================================
    // ZONE 5: OUTER BEAT FLASH RING (70-79 from center) - Purple on beat
    // =========================================================================

    for (uint8_t d = ZONE_TREBLE_END + 1; d < 80; d++) {
        if (m_beatFlashDecay > 0.1f) {
            uint8_t intensity = (uint8_t)(m_beatFlashDecay * ctx.brightness);
            setSymmetricLEDs(ctx, d, CRGB(intensity, 0, intensity));  // Magenta
        } else {
            // Gentle breathing when no beat
            float breath = sinf(m_frameCount * 0.02f) * 0.3f + 0.2f;
            uint8_t dim = (uint8_t)(breath * 30);
            setSymmetricLEDs(ctx, d, CRGB(dim, 0, dim));
        }
    }

    // =========================================================================
    // CENTER PAIR: BPM INDICATOR (alternates based on beat phase)
    // =========================================================================

    // Center pair pulses with beat phase
    uint8_t centerBrightness = (uint8_t)((1.0f - beatPhase) * ctx.brightness);
    setSymmetricLEDs(ctx, 0, CRGB(centerBrightness, centerBrightness, centerBrightness));

#else
    // FEATURE_AUDIO_SYNC is disabled at compile time
    // Show static magenta pattern to indicate this
    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        ctx.leds[i] = CRGB(40, 0, 40);  // Dim magenta
    }

    // Center shows brighter magenta
    setSymmetricLEDs(ctx, 0, CRGB(100, 0, 100));
    setSymmetricLEDs(ctx, 1, CRGB(80, 0, 80));
#endif
}

void TrinityTestEffect::cleanup() {
    // No dynamic resources to free
}

const plugins::EffectMetadata& TrinityTestEffect::getMetadata() const {
    static plugins::EffectMetadata meta(
        "Trinity Test",                                    // Display name
        "Diagnostic effect for PRISM Trinity data flow",  // Description
        plugins::EffectCategory::PARTY,                   // Category (music-reactive)
        1,                                                // Version
        "SpectraSynq"                                     // Author
    );
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
