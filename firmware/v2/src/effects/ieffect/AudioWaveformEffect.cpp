/**
 * @file AudioWaveformEffect.cpp
 * @brief Scrolling waveform visualization with trails and chromagram color
 *
 * CENTER ORIGIN compliant adaptation of SensoryBridge waveform mode.
 * Shows scrolling waveform emanating from center with dynamic trails.
 *
 * Algorithm (original SensoryBridge adapted for CENTER ORIGIN):
 * 1. Apply DYNAMIC FADE to all existing LEDs (creates trails)
 * 2. SHIFT LEDs outward from center (scrolling effect)
 * 3. Get waveform peak amplitude
 * 4. Smooth the peak (5% new, 95% old)
 * 5. Compute color from chromagram
 * 6. Draw new dot at CENTER based on amplitude brightness
 */

#include "AudioWaveformEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:AudioWaveformEffect
namespace {
constexpr float kAudioWaveformEffectSpeedScale = 1.0f;
constexpr float kAudioWaveformEffectOutputGain = 1.0f;
constexpr float kAudioWaveformEffectCentreBias = 1.0f;

float gAudioWaveformEffectSpeedScale = kAudioWaveformEffectSpeedScale;
float gAudioWaveformEffectOutputGain = kAudioWaveformEffectOutputGain;
float gAudioWaveformEffectCentreBias = kAudioWaveformEffectCentreBias;

const lightwaveos::plugins::EffectParameter kAudioWaveformEffectParameters[] = {
    {"audio_waveform_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kAudioWaveformEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"audio_waveform_effect_output_gain", "Output Gain", 0.25f, 2.0f, kAudioWaveformEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"audio_waveform_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kAudioWaveformEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:AudioWaveformEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

static inline const float* selectChroma12(const audio::ControlBusFrame& cb) {
    // Both backends now produce normalised chroma via Stage A/B pipeline.
    return cb.chroma;
}

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

} // namespace

bool AudioWaveformEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:AudioWaveformEffect
    gAudioWaveformEffectSpeedScale = kAudioWaveformEffectSpeedScale;
    gAudioWaveformEffectOutputGain = kAudioWaveformEffectOutputGain;
    gAudioWaveformEffectCentreBias = kAudioWaveformEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:AudioWaveformEffect

    m_peakSmoothed = 0.0f;
    m_sumColorLast[0] = 0.0f;
    m_sumColorLast[1] = 0.0f;
    m_sumColorLast[2] = 0.0f;
    m_chromaAngle = 0.0f;
    m_initialized = false;
    return true;
}

void AudioWaveformEffect::applyDynamicFade(plugins::EffectContext& ctx, float amplitude) {
    // Original SensoryBridge formula:
    // dynamic_fade_amount = 1.0 - (max_fade_reduction * abs_amp)
    // When amplitude is HIGH: less fade (longer trails)
    // When amplitude is LOW: more fade (shorter trails)

    float absAmp = fabsf(amplitude);
    if (absAmp > 1.0f) absAmp = 1.0f;

    // Calculate dynamic fade: base fade reduced by amplitude
    float fadeAmount = BASE_FADE - (MAX_FADE_REDUCTION * absAmp);
    if (fadeAmount < 0.80f) fadeAmount = 0.80f;  // Don't fade too fast
    if (fadeAmount > 0.98f) fadeAmount = 0.98f;  // Always fade a little

    uint8_t fadeScale = (uint8_t)(fadeAmount * 255.0f);

    // Apply fade to ALL LEDs (both strips)
    for (uint16_t i = 0; i < ctx.ledCount; ++i) {
        ctx.leds[i].nscale8(fadeScale);
    }
}

void AudioWaveformEffect::shiftLedsOutward(plugins::EffectContext& ctx) {
    // CENTER ORIGIN shift: LEDs move OUTWARD from center (79/80)
    // This creates the scrolling waveform effect

    uint16_t stripLen = (ctx.ledCount > 160) ? 160 : ctx.ledCount;
    uint16_t center = stripLen / 2;  // 80 for 160 LEDs

    // === STRIP 1: Shift outward from center ===

    // Left half (79 down to 0): shift LEFT (toward 0)
    // LED 0 falls off, LED 1→0, LED 2→1, ..., LED 79→78
    for (uint16_t i = 0; i < center - 1; ++i) {
        ctx.leds[i] = ctx.leds[i + 1];
    }

    // Right half (80 up to 159): shift RIGHT (toward 159)
    // LED 159 falls off, LED 158→159, ..., LED 80→81
    for (uint16_t i = stripLen - 1; i > center; --i) {
        ctx.leds[i] = ctx.leds[i - 1];
    }

    // Clear center pixels (79 and 80) - new data will be drawn here
    ctx.leds[center - 1] = CRGB::Black;  // LED 79
    ctx.leds[center] = CRGB::Black;      // LED 80

    // === STRIP 2: Same pattern if present ===
    if (ctx.ledCount > 160) {
        uint16_t strip2Start = 160;
        uint16_t strip2Center = strip2Start + center;  // 240

        // Left half of strip 2
        for (uint16_t i = strip2Start; i < strip2Center - 1; ++i) {
            ctx.leds[i] = ctx.leds[i + 1];
        }

        // Right half of strip 2
        for (uint16_t i = ctx.ledCount - 1; i > strip2Center; --i) {
            ctx.leds[i] = ctx.leds[i - 1];
        }

        // Clear center pixels for strip 2
        ctx.leds[strip2Center - 1] = CRGB::Black;  // LED 239
        ctx.leds[strip2Center] = CRGB::Black;      // LED 240
    }
}

void AudioWaveformEffect::render(plugins::EffectContext& ctx) {
    // First frame only: clear buffer
    if (!m_initialized) {
        memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
        m_initialized = true;
    }

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        // No audio: just fade existing trails
        applyDynamicFade(ctx, 0.0f);
        return;
    }

    // =========================================
    // STEP 1: Get current audio amplitude
    // Prefer peak waveform amplitude for crisp transients; fall back to RMS.
    // =========================================
    float peak = 0.0f;
    for (uint8_t i = 0; i < ctx.audio.waveformSize(); ++i) {
        float a = ctx.audio.getWaveformAmplitude(i);
        if (a > peak) peak = a;
    }
    float currentAmp = fmaxf(peak, ctx.audio.rms());
    // silentScale handled globally by RendererActor
    currentAmp = clamp01(currentAmp);

    // =========================================
    // STEP 2: Smooth the peak (5%/95% - original)
    // =========================================
    m_peakSmoothed = currentAmp * PEAK_SMOOTH_NEW + m_peakSmoothed * PEAK_SMOOTH_OLD;

    // =========================================
    // STEP 3: Apply DYNAMIC FADE (creates trails)
    // =========================================
    applyDynamicFade(ctx, m_peakSmoothed);

    // =========================================
    // STEP 4: SHIFT LEDs outward from center
    // =========================================
    shiftLedsOutward(ctx);

    // =========================================
    // STEP 5: Compute color from chromagram
    // =========================================
    CRGB dotColor = computeChromaColor(ctx);

    // Scale by smoothed peak amplitude
    uint8_t brightness = (uint8_t)(m_peakSmoothed * 255.0f);
    dotColor.nscale8(brightness);

    // Apply global brightness
    dotColor.nscale8(ctx.brightness);

    // =========================================
    // STEP 6: Draw new dot at CENTER
    // =========================================
    uint16_t stripLen = (ctx.ledCount > 160) ? 160 : ctx.ledCount;
    uint16_t center = stripLen / 2;  // 80

    // Draw at center pair (79 and 80)
    ctx.leds[center - 1] = dotColor;  // LED 79
    ctx.leds[center] = dotColor;      // LED 80

    // Strip 2 center if present
    if (ctx.ledCount > 160) {
        uint16_t strip2Center = 160 + center;  // 240
        ctx.leds[strip2Center - 1] = dotColor;  // LED 239
        ctx.leds[strip2Center] = dotColor;      // LED 240
    }
#endif
}

CRGB AudioWaveformEffect::computeChromaColor(const plugins::EffectContext& ctx) {
    float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;

#if FEATURE_AUDIO_SYNC
    const float* chroma = selectChroma12(ctx.audio.controlBus);

    // Musically anchored palette indices (no HSV hue-wheel sweep).
    // Offsets are deliberately non-linear to avoid a "rainbow ladder" look.
    static constexpr uint8_t NOTE_OFFSETS[12] = {
        0, 12, 28, 40, 58, 72, 92, 108, 132, 152, 176, 204
    };

    // Circular chroma hue (prevents argmax discontinuities and wrapping artefacts).
    float dt = ctx.getSafeRawDeltaSeconds();
    uint8_t baseHue = effects::chroma::circularChromaHueSmoothed(
        chroma, m_chromaAngle, dt, 0.20f);

    // Accumulate color from all chromagram bins
    for (uint8_t c = 0; c < 12; ++c) {
        float bin = chroma[c];

        // Square for contrast, then boost (original algorithm)
        float bright = bin * bin * CHROMA_BOOST;
        if (bright > 1.0f) bright = 1.0f;

        // Only contribute if above threshold
        if (bright > CHROMA_THRESHOLD) {
            uint8_t paletteIdx = (uint8_t)(baseHue + NOTE_OFFSETS[c]);
            uint8_t brightU8 = (uint8_t)(bright * 255.0f);
            // Apply global brightness here to keep per-note contribution bounded.
            brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
            CRGB noteColor = ctx.palette.getColor(paletteIdx, brightU8);

            sumR += noteColor.r;
            sumG += noteColor.g;
            sumB += noteColor.b;
        }
    }
#endif

    // Smooth color (5% new, 95% old - original ratio)
    m_sumColorLast[0] = sumR * COLOR_SMOOTH_NEW + m_sumColorLast[0] * COLOR_SMOOTH_OLD;
    m_sumColorLast[1] = sumG * COLOR_SMOOTH_NEW + m_sumColorLast[1] * COLOR_SMOOTH_OLD;
    m_sumColorLast[2] = sumB * COLOR_SMOOTH_NEW + m_sumColorLast[2] * COLOR_SMOOTH_OLD;

    // Clamp to valid RGB range
    return CRGB(
        (uint8_t)fminf(m_sumColorLast[0], 255.0f),
        (uint8_t)fminf(m_sumColorLast[1], 255.0f),
        (uint8_t)fminf(m_sumColorLast[2], 255.0f)
    );
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:AudioWaveformEffect
uint8_t AudioWaveformEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kAudioWaveformEffectParameters) / sizeof(kAudioWaveformEffectParameters[0]));
}

const plugins::EffectParameter* AudioWaveformEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kAudioWaveformEffectParameters[index];
}

bool AudioWaveformEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "audio_waveform_effect_speed_scale") == 0) {
        gAudioWaveformEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "audio_waveform_effect_output_gain") == 0) {
        gAudioWaveformEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "audio_waveform_effect_centre_bias") == 0) {
        gAudioWaveformEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float AudioWaveformEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "audio_waveform_effect_speed_scale") == 0) return gAudioWaveformEffectSpeedScale;
    if (strcmp(name, "audio_waveform_effect_output_gain") == 0) return gAudioWaveformEffectOutputGain;
    if (strcmp(name, "audio_waveform_effect_centre_bias") == 0) return gAudioWaveformEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:AudioWaveformEffect

void AudioWaveformEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& AudioWaveformEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Audio Waveform",
        "Scrolling waveform from center with dynamic trails - amplitude drives brightness",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
