/**
 * @file LGPChordGlowEffect.cpp
 * @brief LGP Chord Glow - Full chord detection integration showcase effect
 *
 * This effect demonstrates the complete chord detection pipeline:
 * 1. Root note determines base hue (chromatic circle: C=red -> B=violet)
 * 2. Chord type (major/minor/dim/aug) modulates saturation and mood
 * 3. Detection confidence controls overall brightness
 * 4. Chord changes trigger smooth 200ms cross-fade transitions
 * 5. 3rd and 5th intervals appear as accent colors at specific LED positions
 *
 * CENTER ORIGIN: All effects radiate from LED 79/80 (center point) outward.
 */

#include "LGPChordGlowEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Root note to hue mapping (chromatic circle)
// C=0 (red), C#=21, D=42, D#=63, E=84, F=105, F#=126, G=147, G#=168, A=189, A#=210, B=231
static constexpr uint8_t ROOT_NOTE_HUES[12] = {
    0,    // C  - Red
    21,   // C# - Red-Orange
    42,   // D  - Orange
    63,   // D# - Yellow-Orange
    84,   // E  - Yellow
    105,  // F  - Yellow-Green
    126,  // F# - Green
    147,  // G  - Cyan-Green
    168,  // G# - Cyan
    189,  // A  - Blue
    210,  // A# - Blue-Violet
    231   // B  - Violet
};

// Chord mood configurations
static constexpr ChordMood CHORD_MOODS[] = {
    {180, 0, 0.3f},     // NONE - muted, low brightness
    {255, 0, 1.0f},     // MAJOR - high saturation, bright (happy)
    {200, -10, 0.85f},  // MINOR - medium saturation, cooler (melancholic)
    {140, 15, 0.7f},    // DIMINISHED - low saturation, dark (tense)
    {240, 30, 0.95f}    // AUGMENTED - high saturation, ethereal (dreamy)
};

bool LGPChordGlowEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset chord state
    m_currentRootNote = 0;
    m_currentChordType = audio::ChordType::NONE;
    m_currentConfidence = 0.0f;

    m_prevRootNote = 0;
    m_prevChordType = audio::ChordType::NONE;
    m_prevConfidence = 0.0f;

    // Reset transition state
    m_transitionProgress = 1.0f;
    m_isTransitioning = false;

    // Reset smoothing
    m_rootNoteSmooth = 0.0f;
    m_glowPhase = 0.0f;
    m_chordChangePulse = 0.0f;
    m_lastHopSeq = 0;

    return true;
}

void LGPChordGlowEffect::render(plugins::EffectContext& ctx) {
    // Clear output buffer with fade for trails
    const float dt = ctx.getSafeDeltaSeconds();
    const int fadeAmt = (int)roundf(25.0f * (dt * 60.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, clampU8(fadeAmt));

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    // Get chord detection state
    const audio::ChordState& chord = ctx.audio.controlBus.chordState;
    const bool hasAudio = ctx.audio.available;
    const bool newHop = hasAudio && (ctx.audio.controlBus.hop_seq != m_lastHopSeq);

    // Update chord state on new hop
    if (hasAudio && newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;

        // Check for chord change (significant change in root or type)
        bool chordChanged = false;
        if (chord.type != audio::ChordType::NONE && chord.confidence > 0.3f) {
            if (chord.rootNote != m_currentRootNote ||
                chord.type != m_currentChordType) {
                chordChanged = true;

                // Store previous state for blending
                m_prevRootNote = m_currentRootNote;
                m_prevChordType = m_currentChordType;
                m_prevConfidence = m_currentConfidence;

                // Update current state
                m_currentRootNote = chord.rootNote;
                m_currentChordType = chord.type;

                // Start transition
                m_transitionProgress = 0.0f;
                m_isTransitioning = true;

                // Trigger chord change pulse
                m_chordChangePulse = 1.0f;
            }
        }

        // Always update confidence (smoothed)
        m_currentConfidence = smoothValue(m_currentConfidence, chord.confidence, 0.15f);
    }

    // Update transition progress
    if (m_isTransitioning) {
        m_transitionProgress += (ctx.deltaTimeSeconds * 1000.0f / TRANSITION_DURATION_MS);
        if (m_transitionProgress >= 1.0f) {
            m_transitionProgress = 1.0f;
            m_isTransitioning = false;
        }
    }

    // Smooth root note for gradual hue drift
    float targetRoot = (float)m_currentRootNote;
    // Handle wraparound (e.g., transitioning from B to C)
    float diff = targetRoot - m_rootNoteSmooth;
    if (diff > 6.0f) targetRoot -= 12.0f;
    else if (diff < -6.0f) targetRoot += 12.0f;
    m_rootNoteSmooth += (targetRoot - m_rootNoteSmooth) * (1.0f - expf(-dt / 0.2f));  // True exponential, tau=200ms
    // Wrap back to 0-11 range
    while (m_rootNoteSmooth < 0.0f) m_rootNoteSmooth += 12.0f;
    while (m_rootNoteSmooth >= 12.0f) m_rootNoteSmooth -= 12.0f;

    // Update glow phase for ambient animation
    float speedNorm = ctx.speed / 50.0f;
    m_glowPhase += speedNorm * 2.0f * dt;
    if (m_glowPhase > 6.28318f) m_glowPhase -= 6.28318f;

    // Decay chord change pulse
    m_chordChangePulse *= expf(-dt / 0.15f);  // ~150ms decay

    // Get mood parameters for current and previous chord
    ChordMood currentMood = getChordMood(m_currentChordType);
    ChordMood prevMood = getChordMood(m_prevChordType);

    // Calculate blended hue based on transition
    uint8_t currentHue = rootNoteToHue(m_currentRootNote);
    uint8_t prevHue = rootNoteToHue(m_prevRootNote);

    // Get interval notes for accents
    uint8_t thirdNote = getThirdInterval(m_currentRootNote, m_currentChordType);
    uint8_t fifthNote = getFifthInterval(m_currentRootNote, m_currentChordType);
    uint8_t thirdHue = rootNoteToHue(thirdNote);
    uint8_t fifthHue = rootNoteToHue(fifthNote);

    // Calculate positions for accent LEDs (3rd and 5th intervals)
    // Position them at ~33% and ~66% from center
    const uint16_t thirdDist = CHORD_GLOW_HALF_LENGTH / 3;    // ~26 LEDs from center
    const uint16_t fifthDist = (CHORD_GLOW_HALF_LENGTH * 2) / 3;  // ~53 LEDs from center

    // Brightness based on confidence
    float baseBrightness = m_currentConfidence * currentMood.brightnessScale;
    baseBrightness = baseBrightness * (0.6f + 0.4f * (ctx.brightness / 255.0f));

    // Blend saturation during transition
    float blendedSat = prevMood.saturation * (1.0f - m_transitionProgress) +
                       currentMood.saturation * m_transitionProgress;

    // Render CENTER ORIGIN glow pattern
    for (uint16_t dist = 0; dist < CHORD_GLOW_HALF_LENGTH; ++dist) {
        float normalizedDist = (float)dist / (float)CHORD_GLOW_HALF_LENGTH;

        // Base glow intensity (stronger at center, fades toward edges)
        float glow = expf(-normalizedDist * 2.5f);

        // Add pulsing animation
        float pulse = 0.7f + 0.3f * sinf(m_glowPhase - normalizedDist * 3.0f);
        glow *= pulse;

        // Add chord change burst (ripples outward from center)
        if (m_chordChangePulse > 0.01f) {
            float burstDist = m_chordChangePulse * CHORD_GLOW_HALF_LENGTH * 0.5f;
            float burstWidth = 8.0f;
            float burstIntensity = expf(-powf((float)dist - burstDist, 2.0f) / (2.0f * burstWidth * burstWidth));
            glow += burstIntensity * m_chordChangePulse * 0.5f;
        }

        // Calculate hue with transition blending
        uint8_t blendedHue;
        if (m_isTransitioning) {
            // Blend between previous and current hue
            int hueDiff = (int)currentHue - (int)prevHue;
            // Handle wraparound
            if (hueDiff > 128) hueDiff -= 256;
            else if (hueDiff < -128) hueDiff += 256;
            blendedHue = (uint8_t)((int)prevHue + (int)(hueDiff * m_transitionProgress));
        } else {
            blendedHue = currentHue;
        }

        // Apply mood hue offset
        int8_t hueOffset = (int8_t)(prevMood.hueOffset * (1.0f - m_transitionProgress) +
                                     currentMood.hueOffset * m_transitionProgress);
        blendedHue = (uint8_t)((int)blendedHue + hueOffset);

        // Add gHue for palette cycling
        blendedHue += ctx.gHue;

        // Calculate final brightness
        uint8_t brightness = clampU8((int)(glow * baseBrightness * 255.0f));

        // Create base color
        CRGB baseColor = ctx.palette.getColor(blendedHue, brightness);

        // Check for accent positions (3rd and 5th intervals)
        bool isThirdAccent = (dist >= thirdDist - 2 && dist <= thirdDist + 2);
        bool isFifthAccent = (dist >= fifthDist - 2 && dist <= fifthDist + 2);

        if (isThirdAccent && m_currentChordType != audio::ChordType::NONE) {
            // Blend in third interval accent color
            float accentStrength = 0.4f * m_currentConfidence * (1.0f - fabsf((float)dist - (float)thirdDist) / 3.0f);
            uint8_t accentBrightness = clampU8((int)(accentStrength * baseBrightness * 255.0f));
            CRGB accentColor = ctx.palette.getColor(thirdHue + ctx.gHue, accentBrightness);
            baseColor = blendColors(baseColor, accentColor, accentStrength);
        }

        if (isFifthAccent && m_currentChordType != audio::ChordType::NONE) {
            // Blend in fifth interval accent color
            float accentStrength = 0.35f * m_currentConfidence * (1.0f - fabsf((float)dist - (float)fifthDist) / 3.0f);
            uint8_t accentBrightness = clampU8((int)(accentStrength * baseBrightness * 255.0f));
            CRGB accentColor = ctx.palette.getColor(fifthHue + ctx.gHue, accentBrightness);
            baseColor = blendColors(baseColor, accentColor, accentStrength);
        }

        // Apply saturation adjustment
        CHSV hsv = rgb2hsv_approximate(baseColor);
        hsv.s = clampU8((int)(hsv.s * blendedSat / 255.0f));
        hsv2rgb_spectrum(hsv, baseColor);

        // Set symmetric LEDs from center (CENTER ORIGIN pattern)
        SET_CENTER_PAIR(ctx, dist, baseColor);
    }

#endif  // FEATURE_AUDIO_SYNC
}

uint8_t LGPChordGlowEffect::rootNoteToHue(uint8_t rootNote) const {
    if (rootNote >= 12) rootNote = rootNote % 12;
    return ROOT_NOTE_HUES[rootNote];
}

ChordMood LGPChordGlowEffect::getChordMood(audio::ChordType type) const {
    uint8_t idx = static_cast<uint8_t>(type);
    if (idx >= 5) idx = 0;  // Default to NONE
    return CHORD_MOODS[idx];
}

uint8_t LGPChordGlowEffect::getThirdInterval(uint8_t rootNote, audio::ChordType type) const {
    // Major 3rd = +4 semitones, Minor 3rd = +3 semitones
    switch (type) {
        case audio::ChordType::MAJOR:
        case audio::ChordType::AUGMENTED:
            return (rootNote + 4) % 12;
        case audio::ChordType::MINOR:
        case audio::ChordType::DIMINISHED:
            return (rootNote + 3) % 12;
        default:
            return (rootNote + 4) % 12;  // Default to major 3rd
    }
}

uint8_t LGPChordGlowEffect::getFifthInterval(uint8_t rootNote, audio::ChordType type) const {
    // Perfect 5th = +7 semitones
    // Diminished 5th = +6 semitones
    // Augmented 5th = +8 semitones
    switch (type) {
        case audio::ChordType::DIMINISHED:
            return (rootNote + 6) % 12;
        case audio::ChordType::AUGMENTED:
            return (rootNote + 8) % 12;
        default:
            return (rootNote + 7) % 12;  // Perfect 5th
    }
}

float LGPChordGlowEffect::smoothValue(float current, float target, float alpha) const {
    return current + (target - current) * alpha;
}

uint8_t LGPChordGlowEffect::clampU8(int value) const {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

CRGB LGPChordGlowEffect::blendColors(CRGB c1, CRGB c2, float blend) const {
    float inv = 1.0f - blend;
    return CRGB(
        clampU8((int)(c1.r * inv + c2.r * blend)),
        clampU8((int)(c1.g * inv + c2.g * blend)),
        clampU8((int)(c1.b * inv + c2.b * blend))
    );
}

void LGPChordGlowEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPChordGlowEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chord Glow",
        "Musical chord detection showcase: root=hue, type=mood, confidence=brightness, smooth transitions",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
