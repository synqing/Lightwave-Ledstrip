/**
 * @file LGPChordGlowEffect.h
 * @brief LGP Chord Glow - Full chord detection integration showcase effect
 *
 * Demonstrates musical intelligence features:
 * - Root note -> base hue (12 distinct colors for C through B)
 * - Chord type -> saturation/mood (major=bright, minor=cool, dim=dark, aug=ethereal)
 * - Confidence -> brightness (low=dim, high=bright)
 * - Chord changes -> smooth 200ms cross-fade transitions
 * - 3rd/5th intervals -> accent LED positions
 *
 * Effect ID: 75
 * Family: PARTY
 * Tags: CENTER_ORIGIN | AUDIO_SYNC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../audio/contracts/ControlBus.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Constants for chord glow effect
constexpr uint16_t CHORD_GLOW_HALF_LENGTH = 80;
constexpr uint16_t CHORD_GLOW_STRIP_LENGTH = 160;

// Chord type mood parameters
struct ChordMood {
    uint8_t saturation;     // Color saturation
    int8_t hueOffset;       // Hue shift for mood
    float brightnessScale;  // Brightness multiplier
};

class LGPChordGlowEffect : public plugins::IEffect {
public:
    LGPChordGlowEffect() = default;
    ~LGPChordGlowEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Current chord state (smoothed)
    uint8_t m_currentRootNote = 0;      // 0-11 (C=0, B=11)
    audio::ChordType m_currentChordType = audio::ChordType::NONE;
    float m_currentConfidence = 0.0f;

    // Previous chord state (for transitions)
    uint8_t m_prevRootNote = 0;
    audio::ChordType m_prevChordType = audio::ChordType::NONE;
    float m_prevConfidence = 0.0f;

    // Transition state
    float m_transitionProgress = 1.0f;  // 0.0 = start, 1.0 = complete
    static constexpr float TRANSITION_DURATION_MS = 200.0f;
    bool m_isTransitioning = false;

    // Smoothed root note for gradual hue drift
    float m_rootNoteSmooth = 0.0f;

    // Glow animation phase
    float m_glowPhase = 0.0f;

    // Pulse animation for chord changes
    float m_chordChangePulse = 0.0f;

    // Last hop sequence for update detection
    uint32_t m_lastHopSeq = 0;

    // Helper methods
    uint8_t rootNoteToHue(uint8_t rootNote) const;
    ChordMood getChordMood(audio::ChordType type) const;
    uint8_t getThirdInterval(uint8_t rootNote, audio::ChordType type) const;
    uint8_t getFifthInterval(uint8_t rootNote, audio::ChordType type) const;
    float smoothValue(float current, float target, float alpha) const;
    uint8_t clampU8(int value) const;
    CRGB blendColors(CRGB c1, CRGB c2, float blend) const;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
