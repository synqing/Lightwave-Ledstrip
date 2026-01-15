/**
 * @file AudioBandFilter.h
 * @brief Audio band filtering for zone-specific frequency routing (Phase 2b.3)
 *
 * LightwaveOS v2 - Zone Audio Routing
 *
 * This module enables different zones to respond to different frequency bands.
 * Each zone can be configured to react to:
 *   - FULL: All frequencies (default, no filtering)
 *   - BASS: Low frequencies (20-250 Hz)
 *   - MID: Mid frequencies (250-2000 Hz)
 *   - HIGH: High frequencies (2000+ Hz)
 *
 * The filtering is applied to the AudioContext before passing it to the zone's
 * effect render function. This allows effects to use their standard audio
 * accessors (bass(), mid(), treble(), rms()) while receiving only the relevant
 * frequency data for their zone.
 *
 * Band Mapping (8-band ControlBus):
 *   - BASS: bands[0-1] (60-120 Hz) - Sub-bass/Bass
 *   - MID:  bands[2-4] (250 Hz - 1 kHz) - Low-mid/Mid
 *   - HIGH: bands[5-7] (2-7.8 kHz) - High-mid/Presence/Brilliance
 */

#pragma once

#include <cstdint>
#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC
#include "../../plugins/api/EffectContext.h"
#include "../../audio/contracts/ControlBus.h"
#endif

namespace lightwaveos {
namespace zones {

// ==================== Audio Band Definitions ====================

/**
 * @brief Frequency band identifiers for zone audio routing
 *
 * Each zone can be configured to respond to a specific frequency band,
 * enabling multi-zone frequency-separated visualizations where:
 *   - Zone 0 (center) might respond to BASS
 *   - Zone 1 (inner ring) might respond to MID
 *   - Zone 2 (outer ring) might respond to HIGH
 */
namespace AudioBands {
    /// Full spectrum - no filtering, effect receives all audio data
    constexpr uint8_t BAND_FULL = 0;

    /// Bass band (20-250 Hz) - Kick drums, bass guitar, sub
    /// Maps to ControlBus bands[0-1]
    constexpr uint8_t BAND_BASS = 1;

    /// Mid band (250-2000 Hz) - Vocals, guitars, snare
    /// Maps to ControlBus bands[2-4]
    constexpr uint8_t BAND_MID = 2;

    /// High band (2000+ Hz) - Hi-hats, cymbals, presence
    /// Maps to ControlBus bands[5-7]
    constexpr uint8_t BAND_HIGH = 3;

    /// Maximum valid band ID (for validation)
    constexpr uint8_t MAX_BAND = 3;

    /**
     * @brief Validate and clamp band ID to valid range
     * @param band Input band ID
     * @return Valid band ID (0-3), defaults to BAND_FULL if invalid
     */
    inline uint8_t validate(uint8_t band) {
        return (band > MAX_BAND) ? BAND_FULL : band;
    }

    /**
     * @brief Get human-readable name for band
     * @param band Band ID (0-3)
     * @return Band name string
     */
    inline const char* getName(uint8_t band) {
        switch (band) {
            case BAND_FULL: return "Full";
            case BAND_BASS: return "Bass";
            case BAND_MID:  return "Mid";
            case BAND_HIGH: return "High";
            default:        return "Unknown";
        }
    }
}

// ==================== Audio Band Filter ====================

#if FEATURE_AUDIO_SYNC

/**
 * @brief Audio band filter for zone-specific frequency routing
 *
 * This class applies frequency band filtering to an AudioContext,
 * creating a modified context where only the selected band's energy
 * is active. This enables zones to respond to specific frequency ranges.
 *
 * Implementation Strategy:
 * The filter modifies the convenience accessors (bass(), mid(), treble())
 * by zeroing out the irrelevant bands in the ControlBusFrame. Effects
 * using rms() will get a band-specific RMS value, and the 8-band array
 * will have non-target bands zeroed.
 *
 * Example Usage:
 * @code
 * plugins::AudioContext filteredAudio = AudioBandFilter::apply(
 *     originalAudio, AudioBands::BASS);
 * // Now filteredAudio.mid() and filteredAudio.treble() return 0.0f
 * // filteredAudio.bass() and filteredAudio.rms() reflect only bass energy
 * @endcode
 */
class AudioBandFilter {
public:
    /**
     * @brief Apply band filtering to AudioContext
     *
     * Creates a copy of the audio context with non-target bands zeroed out.
     * The filtered context can be safely passed to effects that should only
     * respond to specific frequency ranges.
     *
     * @param source Original AudioContext with full spectrum data
     * @param band Target band (AudioBands::FULL/BASS/MID/HIGH)
     * @return Filtered AudioContext (by value - safe copy)
     *
     * Performance: ~1-2us per call (memcpy + selective zeroing)
     * Memory: Returns by value (stack allocation, ~300 bytes)
     */
    static plugins::AudioContext apply(const plugins::AudioContext& source, uint8_t band) {
        // BAND_FULL = no filtering, return copy as-is
        if (band == AudioBands::BAND_FULL) {
            return source;
        }

        // Create filtered copy
        plugins::AudioContext filtered = source;

        // Apply band-specific filtering to ControlBusFrame
        filterControlBus(filtered.controlBus, band);

        return filtered;
    }

    /**
     * @brief Apply band filtering in-place to an AudioContext
     *
     * More efficient than apply() when you don't need the original.
     * Modifies the context directly without creating a copy.
     *
     * @param ctx AudioContext to filter (modified in place)
     * @param band Target band (AudioBands::FULL/BASS/MID/HIGH)
     */
    static void applyInPlace(plugins::AudioContext& ctx, uint8_t band) {
        if (band == AudioBands::BAND_FULL) {
            return;  // No filtering needed
        }
        filterControlBus(ctx.controlBus, band);
    }

private:
    /**
     * @brief Filter ControlBusFrame bands based on target band
     *
     * Zero out non-target bands and recalculate RMS to reflect
     * only the energy in the target band.
     *
     * Band mapping to 8-band ControlBus:
     *   BASS: bands[0-1] (60-120 Hz)
     *   MID:  bands[2-4] (250 Hz - 1 kHz)
     *   HIGH: bands[5-7] (2-7.8 kHz)
     *
     * @param frame ControlBusFrame to filter (modified in place)
     * @param band Target band
     */
    static void filterControlBus(audio::ControlBusFrame& frame, uint8_t band) {
        // Save target band values first
        float savedBands[audio::CONTROLBUS_NUM_BANDS];
        float savedHeavyBands[audio::CONTROLBUS_NUM_BANDS];

        for (uint8_t i = 0; i < audio::CONTROLBUS_NUM_BANDS; i++) {
            savedBands[i] = frame.bands[i];
            savedHeavyBands[i] = frame.heavy_bands[i];
        }

        // Zero all bands
        for (uint8_t i = 0; i < audio::CONTROLBUS_NUM_BANDS; i++) {
            frame.bands[i] = 0.0f;
            frame.heavy_bands[i] = 0.0f;
        }

        // Determine band range
        uint8_t startIdx = 0;
        uint8_t endIdx = 0;

        switch (band) {
            case AudioBands::BAND_BASS:
                startIdx = 0;
                endIdx = 1;
                break;
            case AudioBands::BAND_MID:
                startIdx = 2;
                endIdx = 4;
                break;
            case AudioBands::BAND_HIGH:
                startIdx = 5;
                endIdx = 7;
                break;
            default:
                return;
        }

        // Restore only target band
        float bandSum = 0.0f;
        uint8_t bandCount = 0;
        for (uint8_t i = startIdx; i <= endIdx && i < audio::CONTROLBUS_NUM_BANDS; i++) {
            frame.bands[i] = savedBands[i];
            frame.heavy_bands[i] = savedHeavyBands[i];
            bandSum += savedBands[i];
            bandCount++;
        }

        // Recalculate RMS to reflect only the filtered band's energy
        // This gives effects using rms() a band-specific amplitude
        if (bandCount > 0) {
            float avgBandEnergy = bandSum / static_cast<float>(bandCount);
            // Scale RMS proportionally to the band's contribution
            // Use the band's average as a proxy for overall energy
            frame.rms = avgBandEnergy;
            frame.fast_rms = avgBandEnergy;
        } else {
            frame.rms = 0.0f;
            frame.fast_rms = 0.0f;
        }

        // Filter flux based on band (spectral flux is band-agnostic in our case,
        // but we scale it by band energy for consistency)
        float originalFlux = frame.flux;
        float originalFastFlux = frame.fast_flux;
        if (bandCount > 0) {
            float avgBandEnergy = bandSum / static_cast<float>(bandCount);
            // Scale flux by band energy ratio (rough approximation)
            frame.flux = originalFlux * avgBandEnergy;
            frame.fast_flux = originalFastFlux * avgBandEnergy;
        }

        // Handle band-specific onset triggers
        switch (band) {
            case AudioBands::BAND_BASS:
                // Keep snare trigger (it's in the bass-mid range ~150-300Hz)
                // Zero hi-hat trigger (it's high frequency)
                frame.hihatTrigger = false;
                frame.hihatEnergy = 0.0f;
                break;
            case AudioBands::BAND_MID:
                // Mid band - keep snare, zero hihat
                frame.hihatTrigger = false;
                frame.hihatEnergy = 0.0f;
                break;
            case AudioBands::BAND_HIGH:
                // High band - keep hihat, zero snare
                frame.snareTrigger = false;
                frame.snareEnergy = 0.0f;
                break;
            default:
                break;
        }
    }
};

#else  // !FEATURE_AUDIO_SYNC

/**
 * @brief Stub AudioBandFilter when audio sync is disabled
 *
 * Provides the same API with no-op implementations so zones compile
 * without #if guards everywhere.
 */
class AudioBandFilter {
public:
    static plugins::AudioContext apply(const plugins::AudioContext& source, uint8_t) {
        return source;  // No filtering without audio
    }

    static void applyInPlace(plugins::AudioContext&, uint8_t) {
        // No-op without audio
    }
};

#endif  // FEATURE_AUDIO_SYNC

} // namespace zones
} // namespace lightwaveos
