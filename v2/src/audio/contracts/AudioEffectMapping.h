/**
 * @file AudioEffectMapping.h
 * @brief Audio-to-Visual Parameter Mapping System
 *
 * LightwaveOS v2 - Phase 4 Audio API Enhancement
 *
 * Provides runtime-configurable audio-to-visual parameter bindings per effect.
 * Maps 19 audio sources to 7 visual targets with 6 response curves.
 *
 * Features:
 * - Per-effect mapping configurations (up to 80 effects)
 * - Up to 8 mappings per effect
 * - IIR smoothing with configurable alpha
 * - 6 curve types: LINEAR, SQUARED, SQRT, LOG, EXP, INVERTED
 * - Additive or replacement mode per mapping
 * - Performance instrumentation
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC

#include <cstdint>
#include <cmath>
#include "ControlBus.h"
#include "MusicalGrid.h"

namespace lightwaveos {
namespace audio {

// =============================================================================
// AUDIO SOURCE ENUMERATION
// =============================================================================

/**
 * @brief Available audio input sources for mapping
 *
 * Sources are grouped by category:
 * - Energy metrics (RMS, flux)
 * - Frequency bands (8 Goertzel bands)
 * - Aggregated bands (bass, mid, treble)
 * - Musical timing (beat phase, BPM)
 */
enum class AudioSource : uint8_t {
    // Energy metrics
    RMS = 0,              ///< Smoothed RMS energy [0,1]
    FAST_RMS = 1,         ///< Fast-attack RMS [0,1]
    FLUX = 2,             ///< Spectral flux (onset) [0,1]
    FAST_FLUX = 3,        ///< Fast-attack flux [0,1]

    // Individual frequency bands (60Hz - 7.8kHz)
    BAND_0 = 4,           ///< 60 Hz - Sub-bass
    BAND_1 = 5,           ///< 120 Hz - Bass
    BAND_2 = 6,           ///< 250 Hz - Low-mid
    BAND_3 = 7,           ///< 500 Hz - Mid
    BAND_4 = 8,           ///< 1000 Hz - High-mid
    BAND_5 = 9,           ///< 2000 Hz - Presence
    BAND_6 = 10,          ///< 4000 Hz - Brilliance
    BAND_7 = 11,          ///< 7800 Hz - Air

    // Aggregated bands
    BASS = 12,            ///< (band0 + band1) / 2
    MID = 13,             ///< (band2 + band3 + band4) / 3
    TREBLE = 14,          ///< (band5 + band6 + band7) / 3
    HEAVY_BASS = 15,      ///< Squared bass response

    // Musical timing
    BEAT_PHASE = 16,      ///< Beat phase [0,1)
    BPM = 17,             ///< Tempo in BPM [30,300]
    TEMPO_CONFIDENCE = 18, ///< Beat detection confidence [0,1]

    NONE = 0xFF           ///< No source (disabled)
};

// =============================================================================
// VISUAL TARGET ENUMERATION
// =============================================================================

/**
 * @brief Available visual output targets for mapping
 *
 * These map directly to EffectContext parameters modified before render().
 */
enum class VisualTarget : uint8_t {
    BRIGHTNESS = 0,       ///< Master LED brightness [0-160]
    SPEED = 1,            ///< Animation speed [1-50]
    INTENSITY = 2,        ///< Effect intensity [0-255]
    SATURATION = 3,       ///< Color saturation [0-255]
    COMPLEXITY = 4,       ///< Pattern complexity [0-255]
    VARIATION = 5,        ///< Effect variation/mode [0-255]
    HUE = 6,              ///< Color hue rotation [0-255]

    NONE = 0xFF           ///< No target (disabled)
};

// =============================================================================
// MAPPING CURVE TYPES
// =============================================================================

/**
 * @brief Response curve types for audio-to-visual mapping
 */
enum class MappingCurve : uint8_t {
    LINEAR = 0,           ///< y = x (direct proportional)
    SQUARED = 1,          ///< y = x² (gentle start, aggressive end)
    SQRT = 2,             ///< y = √x (aggressive start, gentle end)
    LOG = 3,              ///< y = log(x+1)/log(2) (logarithmic)
    EXP = 4,              ///< y = (e^x - 1)/(e - 1) (exponential)
    INVERTED = 5          ///< y = 1 - x (inverse)
};

// =============================================================================
// SINGLE PARAMETER MAPPING
// =============================================================================

/**
 * @brief Configuration for a single audio-to-visual parameter mapping
 */
struct AudioParameterMapping {
    AudioSource source = AudioSource::NONE;
    VisualTarget target = VisualTarget::NONE;
    MappingCurve curve = MappingCurve::LINEAR;

    float inputMin = 0.0f;          ///< Source clip minimum
    float inputMax = 1.0f;          ///< Source clip maximum
    float outputMin = 0.0f;         ///< Target output minimum
    float outputMax = 255.0f;       ///< Target output maximum
    float smoothingAlpha = 0.3f;    ///< IIR coefficient [0.05, 0.95]
    float gain = 1.0f;              ///< Pre-curve multiplier

    bool enabled = false;           ///< Is this mapping active?
    bool additive = false;          ///< Add to existing value (vs replace)

    // Runtime state (not persisted)
    float smoothedValue = 0.0f;

    /**
     * @brief Apply the mapping curve to a normalized input value
     * @param normalizedInput Value in [0,1] range after input clipping
     * @return Curved value in [0,1] range
     */
    float applyCurve(float normalizedInput) const;

    /**
     * @brief Process raw audio input through full mapping pipeline
     * @param rawInput Raw audio value
     * @return Mapped output value in [outputMin, outputMax]
     */
    float apply(float rawInput) const;

    /**
     * @brief Update smoothed value with new input (call each frame)
     * @param rawInput New raw audio value
     */
    void updateSmoothed(float rawInput);

    /**
     * @brief Get the current smoothed and mapped output
     * @return Output value ready for application to visual target
     */
    float getSmoothedOutput() const;
};

// =============================================================================
// PER-EFFECT MAPPING CONFIGURATION
// =============================================================================

/**
 * @brief Complete audio mapping configuration for a single effect
 */
struct EffectAudioMapping {
    static constexpr uint8_t MAX_MAPPINGS_PER_EFFECT = 8;
    static constexpr uint8_t VERSION = 1;

    uint8_t version = VERSION;
    uint8_t effectId = 0xFF;
    bool globalEnabled = false;     ///< Master enable for this effect
    uint8_t mappingCount = 0;
    AudioParameterMapping mappings[MAX_MAPPINGS_PER_EFFECT];
    uint8_t reserved[8] = {0};      ///< Future expansion
    uint32_t checksum = 0;

    /**
     * @brief Calculate CRC32 checksum for validation
     */
    void calculateChecksum();

    /**
     * @brief Validate checksum and version
     */
    bool isValid() const;

    /**
     * @brief Find mapping for a specific visual target
     * @param target Target to find
     * @return Pointer to mapping, or nullptr if not found
     */
    const AudioParameterMapping* findMapping(VisualTarget target) const;
    AudioParameterMapping* findMapping(VisualTarget target);

    /**
     * @brief Add or update a mapping
     * @param mapping Mapping to add
     * @return true if successful (or updated existing)
     */
    bool addMapping(const AudioParameterMapping& mapping);

    /**
     * @brief Remove mapping for a specific target
     * @param target Target to remove
     * @return true if removed
     */
    bool removeMapping(VisualTarget target);

    /**
     * @brief Clear all mappings
     */
    void clearMappings();
};

// =============================================================================
// GLOBAL MAPPING REGISTRY
// =============================================================================

/**
 * @brief Singleton registry managing all effect audio mappings
 *
 * Thread-safe by design: mappings are stored in fixed arrays with
 * atomic operations for enable/disable. The applyMappings() function
 * operates on by-value copies of audio data.
 */
class AudioMappingRegistry {
public:
    static constexpr uint8_t MAX_EFFECTS = 80;

    /**
     * @brief Get singleton instance
     */
    static AudioMappingRegistry& instance();

    // Prevent copying
    AudioMappingRegistry(const AudioMappingRegistry&) = delete;
    AudioMappingRegistry& operator=(const AudioMappingRegistry&) = delete;

    // =========================================================================
    // MAPPING MANAGEMENT
    // =========================================================================

    /**
     * @brief Get mapping configuration for an effect (const)
     * @param effectId Effect index (0-79)
     * @return Pointer to mapping config, or nullptr if invalid ID
     */
    const EffectAudioMapping* getMapping(uint8_t effectId) const;

    /**
     * @brief Get mapping configuration for an effect (mutable)
     * @param effectId Effect index (0-79)
     * @return Pointer to mapping config, or nullptr if invalid ID
     */
    EffectAudioMapping* getMapping(uint8_t effectId);

    /**
     * @brief Set complete mapping configuration for an effect
     * @param effectId Effect index (0-79)
     * @param config Mapping configuration to copy
     * @return true on success
     */
    bool setMapping(uint8_t effectId, const EffectAudioMapping& config);

    /**
     * @brief Enable/disable all mappings for an effect
     * @param effectId Effect index
     * @param enabled Enable state
     */
    void setEffectMappingEnabled(uint8_t effectId, bool enabled);

    /**
     * @brief Check if effect has any active mappings
     * @param effectId Effect index
     * @return true if effect has globalEnabled && mappingCount > 0
     */
    bool hasActiveMappings(uint8_t effectId) const;

    /**
     * @brief Get count of effects with active mappings
     */
    uint8_t getActiveEffectCount() const;

    /**
     * @brief Get total mapping count across all effects
     */
    uint16_t getTotalMappingCount() const;

    // =========================================================================
    // RUNTIME APPLICATION (Called from RendererActor)
    // =========================================================================

    /**
     * @brief Apply audio mappings to visual parameters
     *
     * Called BEFORE effect->render() to modify EffectContext parameters
     * based on current audio state. Operates on by-value audio copies
     * for thread safety.
     *
     * @param effectId Current effect being rendered
     * @param bus Current ControlBusFrame (read-only)
     * @param grid Current MusicalGridSnapshot (read-only)
     * @param audioAvailable Whether audio data is fresh
     * @param[in,out] brightness Modified if mapping exists
     * @param[in,out] speed Modified if mapping exists
     * @param[in,out] intensity Modified if mapping exists
     * @param[in,out] saturation Modified if mapping exists
     * @param[in,out] complexity Modified if mapping exists
     * @param[in,out] variation Modified if mapping exists
     * @param[in,out] hue Modified if mapping exists
     */
    void applyMappings(
        uint8_t effectId,
        const ControlBusFrame& bus,
        const MusicalGridSnapshot& grid,
        bool audioAvailable,
        uint8_t& brightness,
        uint8_t& speed,
        uint8_t& intensity,
        uint8_t& saturation,
        uint8_t& complexity,
        uint8_t& variation,
        uint8_t& hue
    );

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Get human-readable name for audio source
     */
    static const char* getSourceName(AudioSource source);

    /**
     * @brief Get human-readable name for visual target
     */
    static const char* getTargetName(VisualTarget target);

    /**
     * @brief Get human-readable name for curve type
     */
    static const char* getCurveName(MappingCurve curve);

    /**
     * @brief Extract audio value from ControlBusFrame/MusicalGrid by source
     * @param source Audio source enum
     * @param bus ControlBusFrame reference
     * @param grid MusicalGridSnapshot reference
     * @return Audio value (typically 0-1, except BPM which is 30-300)
     */
    static float getAudioValue(
        AudioSource source,
        const ControlBusFrame& bus,
        const MusicalGridSnapshot& grid
    );

    /**
     * @brief Parse audio source from string name
     * @param name Source name (e.g., "BASS", "BEAT_PHASE")
     * @return AudioSource enum, or NONE if not found
     */
    static AudioSource parseSource(const char* name);

    /**
     * @brief Parse visual target from string name
     * @param name Target name (e.g., "BRIGHTNESS", "INTENSITY")
     * @return VisualTarget enum, or NONE if not found
     */
    static VisualTarget parseTarget(const char* name);

    /**
     * @brief Parse curve type from string name
     * @param name Curve name (e.g., "LINEAR", "SQUARED")
     * @return MappingCurve enum, defaults to LINEAR if not found
     */
    static MappingCurve parseCurve(const char* name);

    // =========================================================================
    // PERFORMANCE MONITORING
    // =========================================================================

    /**
     * @brief Get microseconds spent in last applyMappings() call
     */
    uint32_t getLastApplyMicros() const { return m_lastApplyMicros; }

    /**
     * @brief Get total applyMappings() call count
     */
    uint32_t getApplyCount() const { return m_applyCount; }

    /**
     * @brief Get maximum applyMappings() time observed
     */
    uint32_t getMaxApplyMicros() const { return m_maxApplyMicros; }

    /**
     * @brief Reset performance counters
     */
    void resetStats();

private:
    AudioMappingRegistry() = default;

    EffectAudioMapping m_mappings[MAX_EFFECTS];

    // Performance counters
    uint32_t m_applyCount = 0;
    uint32_t m_lastApplyMicros = 0;
    uint32_t m_maxApplyMicros = 0;
    uint64_t m_totalApplyMicros = 0;

    /**
     * @brief Apply a single mapping to its target
     */
    void applySingleMapping(
        AudioParameterMapping& mapping,
        float audioValue,
        uint8_t& targetValue,
        uint8_t minVal,
        uint8_t maxVal
    );
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
