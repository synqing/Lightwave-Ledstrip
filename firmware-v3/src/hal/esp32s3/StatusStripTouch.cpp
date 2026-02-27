#include "hal/esp32s3/StatusStripTouch.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <FastLED.h>

#define LW_LOG_TAG "Touch"
#include "utils/Log.h"

#include "palettes/Palettes_Master.h"

namespace {

// ── Hardware ────────────────────────────────────────────────────────────────
static constexpr uint8_t STATUS_STRIP_PIN  = 38;
static constexpr uint8_t STATUS_LED_COUNT  = 30;
static constexpr uint8_t STATUS_BRIGHTNESS = 50;
static constexpr uint8_t STATUS_CENTRE     = (STATUS_LED_COUNT - 1u) / 2u;  // 14

static_assert(STATUS_LED_COUNT >= 2, "Need at least 2 LEDs for gradient mapping");

// ── Idle modes ──────────────────────────────────────────────────────────────
// MODE_COUNT must match the number of cases in the render switch.
enum IdleMode : uint8_t {
    MODE_STATIC            = 0,  // Clean static gradient
    MODE_SCROLL            = 1,  // Palette travels along the strip
    MODE_BREATHING         = 2,  // Global brightness sine pulse
    MODE_COMET             = 3,  // Asymmetric comet with physics tail
    MODE_PULSE             = 4,  // Centre-origin gaussian halo
    MODE_TWINKLE           = 5,  // Multi-sparkle with fade envelopes
    MODE_ENERGY_BREATHING  = 6,  // Brightness-adaptive breathing
    MODE_WAVE_INTERFERENCE = 7,  // Counter-propagating sine waves
    MODE_COUNT             = 8
};

// ── Pre-computed LUT ────────────────────────────────────────────────────────
static uint8_t baseIndex[STATUS_LED_COUNT];  // palette position per LED [0..240]

// ── LED buffer ──────────────────────────────────────────────────────────────
static CRGB statusLeds[STATUS_LED_COUNT];

// ── Core state ──────────────────────────────────────────────────────────────
static uint8_t  currentPalette  = 255;  // invalid → forces first show to render
static uint8_t  previousPalette = 0;
static uint8_t  idleMode        = MODE_STATIC;
static bool     autoModeActive  = true;
static uint32_t lastUpdateMs    = 0;
static uint32_t phaseEpoch      = 0;    // reset on palette change for clean phase start

// ── Centre-out reveal ───────────────────────────────────────────────────────
static int8_t   revealRadius    = -1;   // -1 = inactive, 0..STATUS_CENTRE+2 = expanding
static uint32_t revealStartMs   = 0;

// ── Comet state ─────────────────────────────────────────────────────────────
static float    cometPos        = 0.0f;
static bool     cometForward    = true;

// ── Multi-sparkle twinkle ───────────────────────────────────────────────────
struct Sparkle {
    int8_t   led     = -1;   // -1 = inactive
    uint32_t startMs = 0;
    uint8_t  peakBri = 0;
};
static constexpr uint8_t MAX_SPARKLES = 3;
static Sparkle  sparkles[MAX_SPARKLES];
static uint32_t lastSparkleMs = 0;

// ── Wave interference ───────────────────────────────────────────────────────
static uint32_t waveAccumA = 0;
static uint32_t waveAccumB = 0;



// ═══════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════

static uint8_t selectModeForPalette(uint8_t paletteIndex) {
    using namespace lightwaveos::palettes;
    if (isPaletteVivid(paletteIndex))  return MODE_SCROLL;
    if (isPaletteCalm(paletteIndex))   return MODE_BREATHING;
    if (isPaletteWarm(paletteIndex))   return MODE_PULSE;
    if (isPaletteCool(paletteIndex))   return MODE_COMET;
    return MODE_ENERGY_BREATHING;
}


// ═══════════════════════════════════════════════════════════════════════════
// Render functions — each populates statusLeds[] from the given palette
// ═══════════════════════════════════════════════════════════════════════════

// Mode 0: Clean static gradient. Shows the palette as-is.
static void renderStaticGradient(const CRGBPalette16& palette, uint8_t brightness) {
    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        statusLeds[i] = ColorFromPalette(palette, baseIndex[i], brightness,
                                          LINEARBLEND);
    }
}

// Mode 1: Smooth palette scroll. Period ~7.7s for a full cycle.
static void renderScroll(uint32_t now, const CRGBPalette16& palette, uint8_t bri) {
    uint8_t scrollPhase = static_cast<uint8_t>((now - phaseEpoch) / 30u);
    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        uint8_t index = static_cast<uint8_t>(baseIndex[i] + scrollPhase);
        statusLeds[i] = ColorFromPalette(palette, index, bri, LINEARBLEND);
    }
}

// Mode 2: Global breathing brightness via sine curve.
static void renderBreathing(uint32_t now, const CRGBPalette16& palette,
                             uint8_t minScale, uint8_t maxScale, uint8_t bri) {
    uint8_t t = sin8(static_cast<uint8_t>((now - phaseEpoch) / 50u));
    uint8_t scale = static_cast<uint8_t>(
        minScale + ((uint16_t)t * (maxScale - minScale) >> 8));
    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        statusLeds[i] = ColorFromPalette(palette, baseIndex[i], bri, LINEARBLEND);
        statusLeds[i].nscale8_video(scale);
    }
}

// Mode 3: Comet with asymmetric tail — sharp gaussian leading edge,
// exponential decay trailing 8-10 LEDs behind.
static void renderComet(uint32_t dt, const CRGBPalette16& palette, uint8_t bri) {
    // Move: ~4 LEDs/sec
    float step = 0.004f * static_cast<float>(dt);  // 4 LEDs/sec in ms
    if (cometForward) {
        cometPos += step;
        if (cometPos >= static_cast<float>(STATUS_LED_COUNT - 1)) {
            cometPos = static_cast<float>(STATUS_LED_COUNT - 1);
            cometForward = false;
        }
    } else {
        cometPos -= step;
        if (cometPos <= 0.0f) {
            cometPos = 0.0f;
            cometForward = true;
        }
    }

    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        CRGB c = ColorFromPalette(palette, baseIndex[i], bri, LINEARBLEND);
        float dist = static_cast<float>(i) - cometPos;
        float absDist = fabsf(dist);
        bool isBehind = (cometForward && dist < 0.0f) ||
                        (!cometForward && dist > 0.0f);

        uint8_t scale;
        if (absDist < 0.5f) {
            // Head: full brightness
            scale = 255u;
        } else if (isBehind && absDist < 10.0f) {
            // Tail: exponential decay (scale8 chain, ~0.7x per LED)
            uint8_t tailDist = static_cast<uint8_t>(absDist);
            uint8_t tailBri = 255u;
            for (uint8_t t = 0; t < tailDist; ++t) {
                tailBri = scale8(tailBri, 178u);  // 178/255 ≈ 0.698
            }
            scale = (tailBri < 90u) ? 90u : tailBri;
        } else if (absDist < 2.5f) {
            // Leading edge: linear falloff over 2 LEDs
            float frac = (absDist - 0.5f) / 2.0f;
            scale = static_cast<uint8_t>(255.0f * (1.0f - frac));
            if (scale < 90u) scale = 90u;
        } else {
            scale = 90u;  // dim base
        }
        c.nscale8_video(scale);
        statusLeds[i] = c;
    }
}

// Mode 4: Centre-origin gaussian halo — sigma breathes via sine.
static void renderPulse(uint32_t now, const CRGBPalette16& palette, uint8_t bri) {
    uint8_t t = sin8(static_cast<uint8_t>((now - phaseEpoch) / 40u));
    // sigma oscillates 2.0 → 7.0 LEDs
    float sigma = 2.0f + static_cast<float>(t) * 5.0f / 255.0f;

    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        CRGB c = ColorFromPalette(palette, baseIndex[i], bri, LINEARBLEND);
        float dist = fabsf(static_cast<float>(i) - static_cast<float>(STATUS_CENTRE));
        float g = expf(-0.5f * (dist / sigma) * (dist / sigma));
        uint8_t scale = static_cast<uint8_t>(90.0f + g * 165.0f);
        c.nscale8_video(scale);
        statusLeds[i] = c;
    }
}

// Mode 5: Multi-sparkle with smooth attack/decay envelopes.
// 3 concurrent sparkle slots, staggered ~800ms apart.
static void renderTwinkle(uint32_t now, const CRGBPalette16& palette, uint8_t bri) {
    // Spawn a new sparkle every ~800ms
    if (now - lastSparkleMs > 800u) {
        lastSparkleMs = now;
        for (uint8_t s = 0; s < MAX_SPARKLES; ++s) {
            if (sparkles[s].led < 0) {
                sparkles[s].led = static_cast<int8_t>(random8(STATUS_LED_COUNT));
                sparkles[s].startMs = now;
                sparkles[s].peakBri = 200u + random8(56u);  // 200–255
                break;
            }
        }
    }

    // Dim base
    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        statusLeds[i] = ColorFromPalette(palette, baseIndex[i], bri, LINEARBLEND);
        statusLeds[i].nscale8_video(100u);
    }

    // Overlay sparkles
    for (uint8_t s = 0; s < MAX_SPARKLES; ++s) {
        if (sparkles[s].led < 0) continue;
        uint32_t age = now - sparkles[s].startMs;
        uint8_t sparkleBri;
        if (age < 50u) {
            // Attack: 0 → peak in 50ms
            sparkleBri = static_cast<uint8_t>(
                (uint16_t)sparkles[s].peakBri * age / 50u);
        } else if (age < 400u) {
            // Decay: peak → 0 in 350ms
            uint32_t decayAge = age - 50u;
            sparkleBri = static_cast<uint8_t>(
                (uint16_t)sparkles[s].peakBri * (350u - decayAge) / 350u);
        } else {
            sparkles[s].led = -1;
            continue;
        }
        uint8_t idx = static_cast<uint8_t>(sparkles[s].led);
        CRGB bright = ColorFromPalette(palette, baseIndex[idx], bri, LINEARBLEND);
        bright.nscale8_video(sparkleBri);
        // Lighten blend: take brighter channel
        statusLeds[idx].r = max(statusLeds[idx].r, bright.r);
        statusLeds[idx].g = max(statusLeds[idx].g, bright.g);
        statusLeds[idx].b = max(statusLeds[idx].b, bright.b);
    }
}

// Mode 6: Energy breathing — speed and amplitude scale with palette brightness.
// Dim palettes breathe slowly (~4s), bright palettes breathe fast (~1s).
static void renderEnergyBreathing(uint32_t now, const CRGBPalette16& palette,
                                   uint8_t avgBrightness, uint8_t bri) {
    uint8_t minScale = 80u;
    uint8_t maxScale = 255u;
    if (avgBrightness < 128u) {
        maxScale = static_cast<uint8_t>(
            180u + ((uint16_t)avgBrightness * 75u >> 7));  // 180–255
    }

    // Speed: dim→2, bright→8. Period dim≈15s, bright≈3.8s.
    uint8_t speedFactor = static_cast<uint8_t>(
        2u + ((uint16_t)avgBrightness * 6u >> 8));
    uint32_t elapsed = now - phaseEpoch;
    // Multiply first for smooth animation (no 150ms staircase stepping).
    // Overflow after ~6 days is acceptable since phaseEpoch resets on palette change.
    uint8_t breathVal = sin8(
        static_cast<uint8_t>((elapsed * static_cast<uint32_t>(speedFactor)) / 150u));
    uint8_t scale = static_cast<uint8_t>(
        minScale + ((uint16_t)breathVal * (maxScale - minScale) >> 8));

    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        statusLeds[i] = ColorFromPalette(palette, baseIndex[i], bri, LINEARBLEND);
        statusLeds[i].nscale8_video(scale);
    }
}

// Mode 7: Two counter-propagating sine waves create drifting interference nodes.
// Vivid palettes get higher spatial frequency.
static void renderWaveInterference(uint32_t dt, const CRGBPalette16& palette,
                                    uint8_t paletteIndex, uint8_t bri) {
    using namespace lightwaveos::palettes;
    waveAccumA += dt * 3u;
    waveAccumB += dt * 4u;
    uint8_t freq = isPaletteVivid(paletteIndex) ? 12u : 8u;

    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        CRGB c = ColorFromPalette(palette, baseIndex[i], bri, LINEARBLEND);
        uint8_t waveA = sin8(static_cast<uint8_t>(
            (waveAccumA >> 8) + i * freq));
        uint8_t waveB = sin8(static_cast<uint8_t>(
            (waveAccumB >> 8) - i * freq));
        uint8_t interference = static_cast<uint8_t>(
            ((uint16_t)waveA + waveB) >> 1);
        // Map [0..255] → [80..255] brightness
        uint8_t scale = static_cast<uint8_t>(
            80u + ((uint16_t)interference * 175u >> 8));
        c.nscale8_video(scale);
        statusLeds[i] = c;
    }
}


// ═══════════════════════════════════════════════════════════════════════════
// Post-processing layers — applied after the idle animation render
// ═══════════════════════════════════════════════════════════════════════════

// Centre-out reveal: new palette expands from centre, old persists at edges.
// Soft 1-LED blend at the boundary via nblend.
static void applyCentreReveal(uint32_t now, uint8_t bri) {
    if (revealRadius < 0) return;

    using namespace lightwaveos::palettes;
    const CRGBPalette16& oldPal =
        gMasterPalettes[validatePaletteId(previousPalette)];

    uint32_t elapsed = now - revealStartMs;
    // ~2 LEDs per 30ms frame = 66 LEDs/sec
    // Clamp before int8_t cast to prevent overflow at elapsed > 1920ms
    uint32_t rawRadius = elapsed / 15u;
    if (rawRadius > STATUS_CENTRE + 2u) {
        revealRadius = -1;  // reveal complete
        return;
    }
    revealRadius = static_cast<int8_t>(rawRadius);

    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        uint8_t dist = (i >= STATUS_CENTRE)
            ? static_cast<uint8_t>(i - STATUS_CENTRE)
            : static_cast<uint8_t>(STATUS_CENTRE - i);
        if (dist == static_cast<uint8_t>(revealRadius)) {
            // Boundary: 50% blend with old palette
            CRGB oldColor = ColorFromPalette(oldPal, baseIndex[i],
                                              bri, LINEARBLEND);
            nblend(statusLeds[i], oldColor, 128u);
        } else if (dist > static_cast<uint8_t>(revealRadius)) {
            // Outside reveal: old palette
            statusLeds[i] = ColorFromPalette(oldPal, baseIndex[i],
                                              bri, LINEARBLEND);
        }
    }
}


}  // namespace


// ═══════════════════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════════════════

void statusStripTouchSetup() {
    auto& ctrl = FastLED.addLeds<WS2812, STATUS_STRIP_PIN, GRB>(statusLeds, STATUS_LED_COUNT);
    // The main strip driver sets FastLED.setBrightness() and setDither() GLOBALLY.
    // Disable dithering on this controller so the global dither doesn't cause
    // flicker on our low-brightness pixels.
    ctrl.setDither(DISABLE_DITHER);

    // Pre-compute gradient LUT: maps LED index → palette position [0..240].
    // CRGBPalette16 has 16 entries at positions 0,16,32,...,240.
    // With LINEARBLEND, indices above 240 wrap toward entry[0], producing
    // wrong colours at the strip endpoints.  Capping at 240 avoids this.
    for (uint8_t i = 0; i < STATUS_LED_COUNT; ++i) {
        baseIndex[i] = static_cast<uint8_t>((i * 240u) / (STATUS_LED_COUNT - 1u));
    }

    // Clear sparkle slots
    for (uint8_t s = 0; s < MAX_SPARKLES; ++s) {
        sparkles[s].led = -1;
    }

    statusStripShowPalette(0);
    LW_LOGI("StatusStrip: pin=%u leds=%u", STATUS_STRIP_PIN, STATUS_LED_COUNT);
}

/// Compute the per-pixel brightness that, after FastLED's global brightness
/// scaling in show(), yields the desired STATUS_BRIGHTNESS on the wire.
/// When globalBri is 255 (no scaling), returns STATUS_BRIGHTNESS unchanged.
/// When globalBri is lower, returns a higher value so the final output stays
/// at STATUS_BRIGHTNESS.  Clamps to 255 to avoid overflow.
static uint8_t compensatedBrightness() {
    uint8_t gb = FastLED.getBrightness();
    if (gb == 0) return 0;
    if (gb >= 255) return STATUS_BRIGHTNESS;
    uint16_t comp = (static_cast<uint16_t>(STATUS_BRIGHTNESS) * 255u + gb / 2u) / gb;
    return (comp > 255u) ? 255u : static_cast<uint8_t>(comp);
}

void statusStripTouchLoop(uint32_t now) {
    using namespace lightwaveos::palettes;

    // Compute dt BEFORE updating lastUpdateMs (fixes the dt=0 bug)
    uint32_t dt = now - lastUpdateMs;

    // ── Animation + post-processing at ~33Hz ────────────────────────────
    if (dt >= 30u) {
        lastUpdateMs = now;

        // Compensate for the global FastLED.setBrightness() that the main
        // strip driver applies.  Without this, our STATUS_BRIGHTNESS=50 gets
        // further scaled by the global value, crushing pixel values to near-zero
        // and causing dark endpoints where palette colours are slightly dimmer.
        uint8_t bri = compensatedBrightness();

        // currentPalette is already validated by statusStripShowPalette
        const CRGBPalette16& palette = gMasterPalettes[currentPalette];
        uint8_t avg = getPaletteAvgBrightness(currentPalette);

        switch (idleMode) {
            case MODE_STATIC:
                renderStaticGradient(palette, bri);
                break;
            case MODE_SCROLL:
                renderScroll(now, palette, bri);
                break;
            case MODE_BREATHING:
                renderBreathing(now, palette, 80u, 255u, bri);
                break;
            case MODE_COMET:
                renderComet(dt, palette, bri);
                break;
            case MODE_PULSE:
                renderPulse(now, palette, bri);
                break;
            case MODE_TWINKLE:
                renderTwinkle(now, palette, bri);
                break;
            case MODE_ENERGY_BREATHING:
                renderEnergyBreathing(now, palette, avg, bri);
                break;
            case MODE_WAVE_INTERFERENCE:
                renderWaveInterference(dt, palette, currentPalette, bri);
                break;
            default:
                renderStaticGradient(palette, bri);
                break;
        }

        // Post-processing
        applyCentreReveal(now, bri);
    }
}

void statusStripShowPalette(uint8_t paletteIndex) {
    using namespace lightwaveos::palettes;
    uint8_t safe = validatePaletteId(paletteIndex);
    if (safe == currentPalette) return;  // no change

    // Start centre-out reveal (only if previous palette was valid)
    if (currentPalette < MASTER_PALETTE_COUNT) {
        previousPalette = currentPalette;
        revealRadius = 0;
        revealStartMs = millis();
    }

    currentPalette = safe;
    phaseEpoch = millis();
    autoModeActive = true;  // re-engage auto-select on palette change

    // Reset stateful animation
    cometPos = 0.0f;
    cometForward = true;
    for (uint8_t s = 0; s < MAX_SPARKLES; ++s) sparkles[s].led = -1;
    lastSparkleMs = 0;
    waveAccumA = 0;
    waveAccumB = 0;

    // Auto-select the most flattering idle mode for this palette
    if (autoModeActive) {
        idleMode = selectModeForPalette(safe);
    }

    // Render initial frame (compensate for global brightness)
    const CRGBPalette16& palette = gMasterPalettes[safe];
    renderStaticGradient(palette, compensatedBrightness());

    LW_LOGI("StatusStrip palette=%u mode=%u", safe, idleMode);
}

void statusStripNextIdleMode() {
    autoModeActive = false;  // manual override until next palette change
    idleMode = static_cast<uint8_t>((idleMode + 1u) % MODE_COUNT);
    LW_LOGI("StatusStrip mode=%u (manual)", idleMode);
}

#endif  // NATIVE_BUILD
