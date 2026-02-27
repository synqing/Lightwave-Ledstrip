// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file EffectContext.h
 * @brief Dependency injection container for effect rendering
 *
 * EffectContext replaces the 15+ global variables from v1 with a single
 * structured container. Effects receive this context in render() and
 * should use ONLY this for accessing LEDs, palettes, and parameters.
 *
 * Key differences from v1:
 * - No global leds[] - use ctx.leds
 * - No global currentPalette - use ctx.palette
 * - No global gHue - use ctx.gHue
 * - No hardcoded 320 - use ctx.ledCount
 * - No hardcoded 80 - use ctx.centerPoint
 *
 * CENTER ORIGIN: Use getDistanceFromCenter(i) for position-based effects.
 * This returns 0.0 at center (LED 79/80) and 1.0 at edges (LED 0/159).
 */

#pragma once

#include <cstdint>
#include <cmath>

// Feature flags
#include "../../config/features.h"

// Forward declare FastLED types for native builds
#ifdef NATIVE_BUILD
#include "../../../test/unit/mocks/fastled_mock.h"
#else
#include <FastLED.h>
#endif

// Audio contracts (Phase 2)
#if FEATURE_AUDIO_SYNC
#include "../../audio/contracts/ControlBus.h"
#include "../../audio/contracts/MusicalGrid.h"
#include "../../audio/contracts/StyleDetector.h"
#endif

// Behavior selection (Phase 3)
#include "BehaviorSelection.h"

namespace lightwaveos {
namespace plugins {

// ============================================================================
// Audio Context (Phase 2)
// ============================================================================

#if FEATURE_AUDIO_SYNC
/**
 * @brief Audio context passed to effects (by-value copies for thread safety)
 *
 * This struct contains copies (not references!) of audio data from the
 * AudioActor. It's populated by RendererActor each frame with extrapolated
 * timing for smooth 120 FPS beat phase.
 *
 * Thread Safety:
 * - All data is copied by value in renderFrame()
 * - No references to AudioActor's buffers
 * - Safe to use throughout effect render()
 */
struct AudioContext {
    audio::ControlBusFrame controlBus;      ///< DSP signals (RMS, flux, bands)
    audio::MusicalGridSnapshot musicalGrid; ///< Beat/tempo tracking
    bool available = false;                  ///< True if audio data is fresh (<100ms old)
    bool trinityActive = false;              ///< True if PRISM Trinity data is active (not microphone)

    // ========================================================================
    // Convenience Accessors
    // ========================================================================

    /// Get RMS energy level (0.0-1.0)
    float rms() const { return controlBus.rms; }
    float fastRms() const { return controlBus.fast_rms; }

    /// Get spectral flux (onset detection signal)
    float flux() const { return controlBus.flux; }
    float fastFlux() const { return controlBus.fast_flux; }

    /// Get frequency band energy (0-7: bass to treble)
    float getBand(uint8_t i) const {
        return (i < audio::CONTROLBUS_NUM_BANDS) ? controlBus.bands[i] : 0.0f;
    }
    float getHeavyBand(uint8_t i) const {
        return (i < audio::CONTROLBUS_NUM_BANDS) ? controlBus.heavy_bands[i] : 0.0f;
    }

    /// Get bass energy (bands 0-1 averaged)
    float bass() const { return (controlBus.bands[0] + controlBus.bands[1]) * 0.5f; }
    float heavyBass() const { return (controlBus.heavy_bands[0] + controlBus.heavy_bands[1]) * 0.5f; }

    /// Get mid energy (bands 2-4 averaged)
    float mid() const {
        return (controlBus.bands[2] + controlBus.bands[3] + controlBus.bands[4]) / 3.0f;
    }
    float heavyMid() const {
        return (controlBus.heavy_bands[2] + controlBus.heavy_bands[3] + controlBus.heavy_bands[4]) / 3.0f;
    }

    /// Get treble energy (bands 5-7 averaged)
    float treble() const {
        return (controlBus.bands[5] + controlBus.bands[6] + controlBus.bands[7]) / 3.0f;
    }
    float heavyTreble() const {
        return (controlBus.heavy_bands[5] + controlBus.heavy_bands[6] + controlBus.heavy_bands[7]) / 3.0f;
    }

    /// Get beat phase (0.0-1.0, wraps each beat)
    float beatPhase() const { return musicalGrid.beat_phase01; }

    /// Check if currently on a beat (single-frame pulse)
    bool isOnBeat() const { return musicalGrid.beat_tick; }

    /// Check if on a downbeat (beat 1 of measure)
    bool isOnDownbeat() const { return musicalGrid.downbeat_tick; }

    /// Get current BPM estimate
    float bpm() const { return musicalGrid.bpm_smoothed; }

    /// Get tempo tracking confidence (0.0-1.0)
    float tempoConfidence() const { return musicalGrid.tempo_confidence; }

    /// Get beat strength (0.0-1.0), peaks on beat detection then decays
    /// Use this to scale visual intensity by beat confidence
    /// Example: brightness *= 0.5f + 0.5f * ctx.audio.beatStrength();
    float beatStrength() const { return musicalGrid.beat_strength; }

    /// Get waveform sample count
    uint8_t waveformSize() const { return audio::CONTROLBUS_WAVEFORM_N; }

    /// Get raw waveform sample at index (int16_t: -32768 to 32767)
    int16_t getWaveformSample(uint8_t index) const {
        if (index < audio::CONTROLBUS_WAVEFORM_N) {
            return controlBus.waveform[index];
        }
        return 0;
    }

    /// Get normalized waveform amplitude at index (0.0-1.0, based on abs(sample)/32768)
    float getWaveformAmplitude(uint8_t index) const {
        if (index < audio::CONTROLBUS_WAVEFORM_N) {
            int16_t sample = controlBus.waveform[index];
            int16_t absSample = (sample < 0) ? -sample : sample;
            return static_cast<float>(absSample) / 32768.0f;
        }
        return 0.0f;
    }

    /// Get normalized waveform sample with sign (-1.0 to +1.0)
    float getWaveformNormalized(uint8_t index) const {
        if (index < audio::CONTROLBUS_WAVEFORM_N) {
            return static_cast<float>(controlBus.waveform[index]) / 32768.0f;
        }
        return 0.0f;
    }

    // ========================================================================
    // Chord Detection Accessors (Priority 6: Musical intelligence)
    // ========================================================================

    /// Get full chord state struct
    const audio::ChordState& chordState() const { return controlBus.chordState; }

    /// Get detected chord type (NONE, MAJOR, MINOR, DIMINISHED, AUGMENTED)
    audio::ChordType chordType() const { return controlBus.chordState.type; }

    /// Get root note (0-11: C=0, C#=1, D=2, ..., B=11)
    uint8_t rootNote() const { return controlBus.chordState.rootNote; }

    /// Get chord detection confidence (0.0-1.0)
    float chordConfidence() const { return controlBus.chordState.confidence; }

    /// Check if detected chord is major
    bool isMajor() const { return controlBus.chordState.type == audio::ChordType::MAJOR; }

    /// Check if detected chord is minor
    bool isMinor() const { return controlBus.chordState.type == audio::ChordType::MINOR; }

    /// Check if detected chord is diminished
    bool isDiminished() const { return controlBus.chordState.type == audio::ChordType::DIMINISHED; }

    /// Check if detected chord is augmented
    bool isAugmented() const { return controlBus.chordState.type == audio::ChordType::AUGMENTED; }

    /// Check if any chord is detected (not NONE)
    bool hasChord() const { return controlBus.chordState.type != audio::ChordType::NONE; }

    // ========================================================================
    // Multi-band Onset Detection Accessors (Phase 1.2: Percussive elements)
    // ========================================================================

    /// Get snare frequency band energy (150-300 Hz, 0.0-1.0)
    float snare() const { return controlBus.snareEnergy; }

    /// Get hi-hat frequency band energy (6-12 kHz, 0.0-1.0)
    float hihat() const { return controlBus.hihatEnergy; }

    /// Check if snare onset detected this frame (single-frame pulse)
    bool isSnareHit() const { return controlBus.snareTrigger; }

    /// Check if hi-hat onset detected this frame (single-frame pulse)
    bool isHihatHit() const { return controlBus.hihatTrigger; }

    // ========================================================================
    // 64-bin FFT Accessors (Phase 2: Detailed frequency analysis)
    // ========================================================================

    /// Get number of FFT bins available
    static constexpr uint8_t bins64Count() { return audio::ControlBusFrame::BINS_64_COUNT; }

    /// Get single bin value from 64-bin FFT (0.0-1.0)
    float bin(uint8_t index) const {
        if (index < audio::ControlBusFrame::BINS_64_COUNT) {
            return controlBus.bins64[index];
        }
        return 0.0f;
    }

    /// Get pointer to full 64-bin array (for efficient iteration)
    const float* bins64() const { return controlBus.bins64; }

    /// Get single bin value from adaptive 64-bin FFT (0.0-1.0)
    float binAdaptive(uint8_t index) const {
        if (index < audio::ControlBusFrame::BINS_64_COUNT) {
            return controlBus.bins64Adaptive[index];
        }
        return 0.0f;
    }

    /// Get pointer to adaptive 64-bin array (Sensory Bridge normalisation)
    const float* bins64Adaptive() const { return controlBus.bins64Adaptive; }

    // ========================================================================
    // Musical Saliency Accessors (MIS Phase 1: Adaptive audio-visual intelligence)
    // ========================================================================

    /// Get full saliency frame struct
    const audio::MusicalSaliencyFrame& saliencyFrame() const { return controlBus.saliency; }

    /// Get overall saliency score (0.0-1.0, weighted combination of all novelty types)
    float overallSaliency() const { return controlBus.saliency.overallSaliency; }

    /// Check if harmonic saliency is currently dominant (chord/key changes)
    bool isHarmonicDominant() const {
        return controlBus.saliency.getDominantType() == audio::SaliencyType::HARMONIC;
    }

    /// Check if rhythmic saliency is currently dominant (beat/tempo changes)
    bool isRhythmicDominant() const {
        return controlBus.saliency.getDominantType() == audio::SaliencyType::RHYTHMIC;
    }

    /// Check if timbral saliency is currently dominant (spectral/texture changes)
    bool isTimbralDominant() const {
        return controlBus.saliency.getDominantType() == audio::SaliencyType::TIMBRAL;
    }

    /// Check if dynamic saliency is currently dominant (loudness/energy changes)
    bool isDynamicDominant() const {
        return controlBus.saliency.getDominantType() == audio::SaliencyType::DYNAMIC;
    }

    /// Get harmonic saliency (smoothed, 0.0-1.0) - chord/key changes
    float harmonicSaliency() const { return controlBus.saliency.harmonicNoveltySmooth; }

    /// Get rhythmic saliency (smoothed, 0.0-1.0) - beat pattern changes
    float rhythmicSaliency() const { return controlBus.saliency.rhythmicNoveltySmooth; }

    /// Get timbral saliency (smoothed, 0.0-1.0) - spectral character changes
    float timbralSaliency() const { return controlBus.saliency.timbralNoveltySmooth; }

    /// Get dynamic saliency (smoothed, 0.0-1.0) - loudness envelope changes
    float dynamicSaliency() const { return controlBus.saliency.dynamicNoveltySmooth; }

    // ========================================================================
    // Music Style Accessors (MIS Phase 2: Adaptive style detection)
    // ========================================================================

    /// Get detected music style
    audio::MusicStyle musicStyle() const { return controlBus.currentStyle; }

    /// Get style detection confidence (0.0-1.0)
    float styleConfidence() const { return controlBus.styleConfidence; }

    /// Check if music is rhythm-driven (EDM, hip-hop)
    bool isRhythmicMusic() const { return controlBus.currentStyle == audio::MusicStyle::RHYTHMIC_DRIVEN; }

    /// Check if music is harmony-driven (jazz, classical)
    bool isHarmonicMusic() const { return controlBus.currentStyle == audio::MusicStyle::HARMONIC_DRIVEN; }

    /// Check if music is melody-driven (vocal pop)
    bool isMelodicMusic() const { return controlBus.currentStyle == audio::MusicStyle::MELODIC_DRIVEN; }

    /// Check if music is texture-driven (ambient, drone)
    bool isTextureMusic() const { return controlBus.currentStyle == audio::MusicStyle::TEXTURE_DRIVEN; }

    /// Check if music is dynamics-driven (orchestral)
    bool isDynamicMusic() const { return controlBus.currentStyle == audio::MusicStyle::DYNAMIC_DRIVEN; }

    // ========================================================================
    // Behavior Context Accessors (MIS Phase 3: Adaptive behavior selection)
    // ========================================================================

    BehaviorContext behaviorContext{};  ///< Behavior selection context (populated from AudioActor)

    /// Get the recommended primary visual behavior
    VisualBehavior recommendedBehavior() const { return behaviorContext.recommendedPrimary; }

    /// Check if effect should pulse on beat (rhythmic music or high rhythmic saliency)
    bool shouldPulseOnBeat() const {
        return behaviorContext.recommendedPrimary == VisualBehavior::PULSE_ON_BEAT;
    }

    /// Check if effect should drift with harmony (harmonic music or chord changes)
    bool shouldDriftWithHarmony() const {
        return behaviorContext.recommendedPrimary == VisualBehavior::DRIFT_WITH_HARMONY;
    }

    /// Check if effect should shimmer with melody (melodic music or treble emphasis)
    bool shouldShimmerWithMelody() const {
        return behaviorContext.recommendedPrimary == VisualBehavior::SHIMMER_WITH_MELODY;
    }

    /// Check if effect should breathe with dynamics (dynamic music or RMS-driven)
    bool shouldBreatheWithDynamics() const {
        return behaviorContext.recommendedPrimary == VisualBehavior::BREATHE_WITH_DYNAMICS;
    }

    /// Check if effect should use texture flow (ambient/textural music)
    bool shouldTextureFlow() const {
        return behaviorContext.recommendedPrimary == VisualBehavior::TEXTURE_FLOW;
    }
};

#else
/**
 * @brief Stub AudioContext when FEATURE_AUDIO_SYNC is disabled
 *
 * Provides the same API with sensible defaults so effects compile
 * without #if guards everywhere.
 */
struct AudioContext {
    bool available = false;
    bool trinityActive = false;

    float rms() const { return 0.0f; }
    float fastRms() const { return 0.0f; }
    float flux() const { return 0.0f; }
    float fastFlux() const { return 0.0f; }
    float getBand(uint8_t) const { return 0.0f; }
    float getHeavyBand(uint8_t) const { return 0.0f; }
    float bass() const { return 0.0f; }
    float heavyBass() const { return 0.0f; }
    float mid() const { return 0.0f; }
    float heavyMid() const { return 0.0f; }
    float treble() const { return 0.0f; }
    float heavyTreble() const { return 0.0f; }
    float beatPhase() const { return 0.0f; }
    bool isOnBeat() const { return false; }
    bool isOnDownbeat() const { return false; }
    float bpm() const { return 120.0f; }
    float tempoConfidence() const { return 0.0f; }
    float beatStrength() const { return 0.0f; }
    uint8_t waveformSize() const { return 128; }
    int16_t getWaveformSample(uint8_t) const { return 0; }
    float getWaveformAmplitude(uint8_t) const { return 0.0f; }
    float getWaveformNormalized(uint8_t) const { return 0.0f; }

    // Chord detection stubs (always return "no chord")
    struct StubChordState {
        uint8_t rootNote = 0;
        uint8_t type = 0;  // NONE
        float confidence = 0.0f;
        float rootStrength = 0.0f;
        float thirdStrength = 0.0f;
        float fifthStrength = 0.0f;
    };
    StubChordState chordState() const { return StubChordState{}; }
    uint8_t chordType() const { return 0; }  // NONE
    uint8_t rootNote() const { return 0; }
    float chordConfidence() const { return 0.0f; }
    bool isMajor() const { return false; }
    bool isMinor() const { return false; }
    bool isDiminished() const { return false; }
    bool isAugmented() const { return false; }
    bool hasChord() const { return false; }

    // Multi-band onset stubs (always return 0/false)
    float snare() const { return 0.0f; }
    float hihat() const { return 0.0f; }
    bool isSnareHit() const { return false; }
    bool isHihatHit() const { return false; }

    // 64-bin FFT stubs
    static constexpr uint8_t bins64Count() { return 64; }
    float bin(uint8_t) const { return 0.0f; }
    const float* bins64() const { return nullptr; }
    float binAdaptive(uint8_t) const { return 0.0f; }
    const float* bins64Adaptive() const { return nullptr; }

    // Musical saliency stubs (always return "not salient")
    struct StubSaliencyFrame {
        float overallSaliency = 0.0f;
        float harmonicNoveltySmooth = 0.0f;
        float rhythmicNoveltySmooth = 0.0f;
        float timbralNoveltySmooth = 0.0f;
        float dynamicNoveltySmooth = 0.0f;
        uint8_t dominantType = 3;  // DYNAMIC default
    };
    StubSaliencyFrame saliencyFrame() const { return StubSaliencyFrame{}; }
    float overallSaliency() const { return 0.0f; }
    bool isHarmonicDominant() const { return false; }
    bool isRhythmicDominant() const { return false; }
    bool isTimbralDominant() const { return false; }
    bool isDynamicDominant() const { return false; }
    float harmonicSaliency() const { return 0.0f; }
    float rhythmicSaliency() const { return 0.0f; }
    float timbralSaliency() const { return 0.0f; }
    float dynamicSaliency() const { return 0.0f; }

    // Music style stubs
    uint8_t musicStyle() const { return 0; }  // UNKNOWN
    float styleConfidence() const { return 0.0f; }
    bool isRhythmicMusic() const { return false; }
    bool isHarmonicMusic() const { return false; }
    bool isMelodicMusic() const { return false; }
    bool isTextureMusic() const { return false; }
    bool isDynamicMusic() const { return false; }

    // Behavior context stubs (always return default behavior)
    BehaviorContext behaviorContext{};  ///< Default behavior context
    VisualBehavior recommendedBehavior() const { return VisualBehavior::BREATHE_WITH_DYNAMICS; }
    bool shouldPulseOnBeat() const { return false; }
    bool shouldDriftWithHarmony() const { return false; }
    bool shouldShimmerWithMelody() const { return false; }
    bool shouldBreatheWithDynamics() const { return true; }  // Default behavior
    bool shouldTextureFlow() const { return false; }
};
#endif

/**
 * @brief Palette wrapper for portable color lookups
 */
class PaletteRef {
public:
    PaletteRef() : m_palette(nullptr) {}

#ifndef NATIVE_BUILD
    explicit PaletteRef(const CRGBPalette16* palette) : m_palette(palette) {}

    /**
     * @brief Get color from palette
     * @param index Position in palette (0-255)
     * @param brightness Optional brightness scaling (0-255)
     * @return Color from palette
     */
    CRGB getColor(uint8_t index, uint8_t brightness = 255) const {
        if (!m_palette) return CRGB::Black;
        return ColorFromPalette(*m_palette, index, brightness, LINEARBLEND);
    }
#else
    explicit PaletteRef(const void* palette) : m_palette(palette) {}

    CRGB getColor(uint8_t index, uint8_t brightness = 255) const {
        (void)brightness;
        // Mock implementation for testing
        return CRGB(index, index, index);
    }
#endif

    bool isValid() const { return m_palette != nullptr; }
    
    /**
     * @brief Get raw palette pointer (for adapter compatibility)
     * @return Raw palette pointer
     */
#ifndef NATIVE_BUILD
    const CRGBPalette16* getRaw() const { return m_palette; }
#else
    const void* getRaw() const { return m_palette; }
#endif

private:
#ifndef NATIVE_BUILD
    const CRGBPalette16* m_palette;
#else
    const void* m_palette;
#endif
};

/**
 * @brief Effect rendering context with all dependencies
 *
 * This is the single source of truth for effect rendering. All effect
 * implementations receive this context and should NOT access any other
 * global state.
 */
struct EffectContext {
    //--------------------------------------------------------------------------
    // LED Buffer (WRITE TARGET)
    //--------------------------------------------------------------------------

    CRGB* leds;                 ///< LED buffer to write to
    uint16_t ledCount;          ///< Total LED count (320 for standard config)
    uint16_t centerPoint;       ///< CENTER ORIGIN point (80 for standard config)

    //--------------------------------------------------------------------------
    // Palette
    //--------------------------------------------------------------------------

    PaletteRef palette;         ///< Current palette for color lookups

    //--------------------------------------------------------------------------
    // Global Animation Parameters
    //--------------------------------------------------------------------------

    uint8_t brightness;         ///< Master brightness (0-255)
    uint8_t speed;              ///< Animation speed (1-100)
    uint8_t gHue;               ///< Auto-incrementing hue (0-255)
    uint8_t mood;               ///< Sensory Bridge mood (0-255): low=reactive, high=smooth

    //--------------------------------------------------------------------------
    // Visual Enhancement Parameters (from v1 ColorEngine)
    //--------------------------------------------------------------------------

    uint8_t intensity;          ///< Effect intensity (0-255)
    uint8_t saturation;         ///< Color saturation (0-255)
    uint8_t complexity;         ///< Pattern complexity (0-255)
    uint8_t variation;          ///< Random variation (0-255)
    uint8_t fadeAmount;         ///< Trail fade (0-255): 0=no fade, higher=faster

    //--------------------------------------------------------------------------
    // Timing
    //--------------------------------------------------------------------------

    uint32_t deltaTimeMs;       ///< Time since last frame (ms)
    float deltaTimeSeconds;     ///< Time since last frame (seconds, high precision)
    uint32_t frameNumber;       ///< Frame counter (wraps at 2^32)
    uint32_t totalTimeMs;       ///< Total effect runtime (ms)

    //--------------------------------------------------------------------------
    // Zone Information (when rendering a zone)
    //--------------------------------------------------------------------------

    uint8_t zoneId;             ///< Current zone ID (0-3, or 0xFF if global)
    uint16_t zoneStart;         ///< Zone start index in global buffer
    uint16_t zoneLength;        ///< Zone length

    //--------------------------------------------------------------------------
    // Audio Context (Phase 2 - Audio Sync)
    //--------------------------------------------------------------------------

    AudioContext audio;         ///< Audio-reactive data (by-value copy)

    //--------------------------------------------------------------------------
    // Helper Methods
    //--------------------------------------------------------------------------

    /**
     * @brief Calculate normalized distance from center (CENTER ORIGIN pattern)
     * @param index LED index (0 to ledCount-1)
     * @return Distance from center: 0.0 at center, 1.0 at edges
     *
     * This is the core method for CENTER ORIGIN compliance. Effects should
     * use this instead of raw index for position-based calculations.
     *
     * Example:
     * @code
     * for (uint16_t i = 0; i < ctx.ledCount; i++) {
     *     float dist = ctx.getDistanceFromCenter(i);
     *     uint8_t heat = 255 * (1.0f - dist);  // Hotter at center
     *     ctx.leds[i] = ctx.palette.getColor(heat);
     * }
     * @endcode
     */
    float getDistanceFromCenter(uint16_t index) const {
        if (ledCount == 0 || centerPoint == 0) return 0.0f;

        int16_t distanceFromCenter = abs((int16_t)index - (int16_t)centerPoint);
        float maxDistance = (float)centerPoint;  // Distance to edge

        return (float)distanceFromCenter / maxDistance;
    }

    /**
     * @brief Get signed position from center (-1.0 to +1.0)
     * @param index LED index
     * @return -1.0 at start, 0.0 at center, +1.0 at end
     *
     * Useful for effects that need to know which "side" of center an LED is on.
     */
    float getSignedPosition(uint16_t index) const {
        if (ledCount == 0 || centerPoint == 0) return 0.0f;

        int16_t offset = (int16_t)index - (int16_t)centerPoint;
        float maxOffset = (float)centerPoint;

        return (float)offset / maxOffset;
    }

    /**
     * @brief Map strip index to mirror position (for symmetric effects)
     * @param index LED index on one side
     * @return Corresponding index on the other side
     *
     * For a 320-LED strip with center at 80:
     * - mirrorIndex(0) returns 159
     * - mirrorIndex(79) returns 80
     * - mirrorIndex(80) returns 79
     */
    uint16_t mirrorIndex(uint16_t index) const {
        if (index >= ledCount) return 0;

        if (index < centerPoint) {
            // Left side -> right side
            return centerPoint + (centerPoint - 1 - index);
        } else {
            // Right side -> left side
            return centerPoint - 1 - (index - centerPoint);
        }
    }

    /**
     * @brief Get time-based phase for smooth animations
     * @param frequencyHz Oscillation frequency
     * @return Phase value (0.0 to 1.0)
     */
    float getPhase(float frequencyHz) const {
        float period = 1000.0f / frequencyHz;
        return fmodf((float)totalTimeMs, period) / period;
    }

    /**
     * @brief Get sine wave value based on time
     * @param frequencyHz Oscillation frequency
     * @return Sine value (-1.0 to +1.0)
     */
    float getSineWave(float frequencyHz) const {
        float phase = getPhase(frequencyHz);
        return sinf(phase * 2.0f * 3.14159265f);
    }

    /**
     * @brief Check if this is a zone render (not full strip)
     * @return true if rendering to a zone
     */
    bool isZoneRender() const {
        return zoneId != 0xFF;
    }

    /**
     * @brief Get normalized mood value (Sensory Bridge pattern)
     * @return 0.0 (reactive) to 1.0 (smooth)
     *
     * Effects can use this to adjust their smoothing behavior:
     * - Low values: More responsive to transients, faster attack/decay
     * - High values: More sustained, slower smoothing, dreamier feel
     *
     * Example (asymmetric smoothing):
     * @code
     * float moodNorm = ctx.getMoodNormalized();
     * float riseAlpha = 0.3f + 0.4f * moodNorm;  // 0.3-0.7
     * float fallAlpha = 0.5f + 0.3f * moodNorm;  // 0.5-0.8
     * @endcode
     */
    float getMoodNormalized() const {
        return static_cast<float>(mood) / 255.0f;
    }

    /**
     * @brief Get smoothing follower coefficients (Sensory Bridge pattern)
     * @param riseOut Output: rise/attack coefficient (0.0-1.0)
     * @param fallOut Output: fall/decay coefficient (0.0-1.0)
     *
     * Implements the Sensory Bridge MOOD knob smoothing_follower behavior:
     * - Low mood: Fast rise (0.3), faster fall (0.5) - reactive
     * - High mood: Slow rise (0.7), slower fall (0.8) - smooth
     */
    void getMoodSmoothing(float& riseOut, float& fallOut) const {
        float moodNorm = getMoodNormalized();
        riseOut = 0.3f + 0.4f * moodNorm;   // 0.3 (reactive) to 0.7 (smooth)
        fallOut = 0.5f + 0.3f * moodNorm;   // 0.5 (reactive) to 0.8 (smooth)
    }

    /**
     * @brief Get safe delta time in seconds (clamped for physics stability)
     * @return Delta time in seconds, clamped to [0.0001, 0.05]
     *
     * This prevents physics explosion on frame drops (>50ms) and ensures
     * a minimum timestep for stability (0.1ms). Essential for true exponential
     * smoothing formulas: alpha = 1 - exp(-lambda * dt)
     *
     * Example:
     * @code
     * float dt = ctx.getSafeDeltaSeconds();
     * float alpha = 1.0f - expf(-5.0f * dt);  // True exponential decay
     * value += (target - value) * alpha;
     * @endcode
     */
    float getSafeDeltaSeconds() const {
        float dt = deltaTimeSeconds;
        if (dt < 0.0001f) dt = 0.0001f;   // Minimum 0.1ms for slow-motion precision
        if (dt > 0.05f) dt = 0.05f;     // Maximum 50ms (20 FPS floor)
        return dt;
    }

    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------

    EffectContext()
        : leds(nullptr)
        , ledCount(0)
        , centerPoint(0)
        , palette()
        , brightness(255)
        , speed(15)
        , gHue(0)
        , mood(128)  // Default 0.5 normalized: balanced reactive/smooth
        , intensity(128)
        , saturation(255)
        , complexity(128)
        , variation(64)
        , fadeAmount(20)
        , deltaTimeMs(8)
        , deltaTimeSeconds(0.008f)
        , frameNumber(0)
        , totalTimeMs(0)
        , zoneId(0xFF)
        , zoneStart(0)
        , zoneLength(0)
        , audio()  // Default-initialized (available=false)
    {}
};

} // namespace plugins
} // namespace lightwaveos
