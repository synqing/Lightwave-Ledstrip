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
#endif

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

    // ========================================================================
    // Convenience Accessors
    // ========================================================================

    /// Get RMS energy level (0.0-1.0)
    float rms() const { return controlBus.rms; }

    /// Get spectral flux (onset detection signal)
    float flux() const { return controlBus.flux; }

    /// Get frequency band energy (0-7: bass to treble)
    float getBand(uint8_t i) const {
        return (i < audio::CONTROLBUS_NUM_BANDS) ? controlBus.bands[i] : 0.0f;
    }

    /// Get bass energy (bands 0-1 averaged)
    float bass() const { return (controlBus.bands[0] + controlBus.bands[1]) * 0.5f; }

    /// Get mid energy (bands 2-4 averaged)
    float mid() const {
        return (controlBus.bands[2] + controlBus.bands[3] + controlBus.bands[4]) / 3.0f;
    }

    /// Get treble energy (bands 5-7 averaged)
    float treble() const {
        return (controlBus.bands[5] + controlBus.bands[6] + controlBus.bands[7]) / 3.0f;
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
};

#else
/**
 * @brief Stub AudioContext when FEATURE_AUDIO_SYNC is disabled
 *
 * Provides the same API with sensible defaults so effects compile
 * without #if guards everywhere.
 */

/// Stub ControlBusFrame for disabled audio sync
struct StubControlBusFrame {
    uint32_t hop_seq = 0;
    float rms = 0.0f;
    float flux = 0.0f;
    float bands[8] = {0};
    float chroma[12] = {0};
    int16_t waveform[128] = {0};
};

/// Stub MusicalGridSnapshot for disabled audio sync
struct StubMusicalGridSnapshot {
    float beat_phase01 = 0.0f;
    bool beat_tick = false;
    bool downbeat_tick = false;
    float bpm_smoothed = 120.0f;
    float tempo_confidence = 0.0f;
};

struct AudioContext {
    StubControlBusFrame controlBus;      ///< Stub DSP signals
    StubMusicalGridSnapshot musicalGrid; ///< Stub beat/tempo
    bool available = false;

    float rms() const { return 0.0f; }
    float flux() const { return 0.0f; }
    float getBand(uint8_t) const { return 0.0f; }
    float bass() const { return 0.0f; }
    float mid() const { return 0.0f; }
    float treble() const { return 0.0f; }
    float beatPhase() const { return 0.0f; }
    bool isOnBeat() const { return false; }
    bool isOnDownbeat() const { return false; }
    float bpm() const { return 120.0f; }
    float tempoConfidence() const { return 0.0f; }
    uint8_t waveformSize() const { return 128; }
    int16_t getWaveformSample(uint8_t) const { return 0; }
    float getWaveformAmplitude(uint8_t) const { return 0.0f; }
    float getWaveformNormalized(uint8_t) const { return 0.0f; }
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
    uint8_t speed;              ///< Animation speed (1-50)
    uint8_t gHue;               ///< Auto-incrementing hue (0-255)

    //--------------------------------------------------------------------------
    // Visual Enhancement Parameters (from v1 ColorEngine)
    //--------------------------------------------------------------------------

    uint8_t intensity;          ///< Effect intensity (0-255)
    uint8_t saturation;         ///< Color saturation (0-255)
    uint8_t complexity;         ///< Pattern complexity (0-255)
    uint8_t variation;          ///< Random variation (0-255)

    //--------------------------------------------------------------------------
    // Timing
    //--------------------------------------------------------------------------

    uint32_t deltaTimeMs;       ///< Time since last frame (ms)
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
        , intensity(128)
        , saturation(255)
        , complexity(128)
        , variation(64)
        , deltaTimeMs(8)
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
