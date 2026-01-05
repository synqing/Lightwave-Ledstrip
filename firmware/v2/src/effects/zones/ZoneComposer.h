/**
 * @file ZoneComposer.h
 * @brief Multi-zone effect orchestration with buffer proxy pattern
 *
 * LightwaveOS v2 - Zone System
 *
 * The ZoneComposer manages independent effect rendering across 1-4 concentric
 * zones with per-zone control of effect, brightness, speed, palette, and blend mode.
 *
 * Architecture:
 * 1. Each zone has its own effect, rendered to a temp buffer
 * 2. Zone segments are extracted from the full render
 * 3. Zones are composited using blend modes
 * 4. Final output is written to the main LED buffer
 *
 * Phase 2c.3 Optimizations:
 * - Pre-allocated zone buffers (already in place)
 * - Cached segment bounds (recalculated only on layout change)
 * - Pre-fetched blend function pointers (O(1) dispatch)
 * - Early zone skip for disabled zones
 * - Estimated savings: ~190 us per frame
 */

#pragma once

#include <FastLED.h>
#include <functional>
#include "ZoneDefinition.h"
#include "BlendMode.h"
#include "AudioBandFilter.h"  // For AudioBands::BAND_* constants
#include "../../core/actors/RendererNode.h"
#include "../../plugins/api/EffectContext.h"

// Forward declaration for audio context
namespace lightwaveos { namespace plugins { struct AudioContext; } }


namespace lightwaveos {
namespace zones {

using namespace lightwaveos::nodes;

// ==================== Zone State Callback ====================

/**
 * @brief Callback type for zone state changes
 *
 * Invoked whenever a zone's state is modified (effect, brightness, speed, etc.)
 * Used by WebServer to broadcast real-time updates to WebSocket clients.
 *
 * @param zoneId The zone that changed (0-3)
 */
using ZoneStateCallback = std::function<void(uint8_t zoneId)>;

// Use the EffectRenderFn typedef from RendererNode
using EffectFunc = EffectRenderFn;

// ==================== Memory Metrics (Phase 2c.2) ====================

/**
 * @brief Memory footprint metrics for zone system
 *
 * Exposes zone system RAM usage for debugging and optimization.
 * Calculated from struct sizes and buffer allocations.
 */
struct ZoneMemoryStats {
    size_t configSize;           // Per-zone config storage (ZoneState * MAX_ZONES)
    size_t bufferSize;           // LED buffer bytes (TOTAL_LEDS * 3 * MAX_ZONES)
    size_t totalZoneBytes;       // Total zone system RAM footprint
    size_t composerOverhead;     // ZoneComposer struct size
    size_t presetStorageMax;     // Max NVS usage for presets
    uint8_t activeZones;         // Currently enabled zone count
    size_t heapFree;             // ESP free heap at query time
    size_t heapLargestBlock;     // Largest contiguous free block
};

// ==================== Timing Metrics ====================

/**
 * @brief Performance timing metrics for zone composition
 *
 * Tracks per-zone render times, blend overhead, and frame skip detection.
 * Used for performance monitoring and optimization baseline.
 */
struct ZoneTimingMetrics {
    uint32_t zoneRenderUs[MAX_ZONES];   // Per-zone effect render time in microseconds
    uint32_t zoneBlendUs;                // Time to composite zones (blend step)
    uint32_t zoneTotalUs;                // Total zone system overhead
    uint32_t frameSkipCount;             // Frames where zones were skipped due to timing
    uint32_t lastUpdateMs;               // Timestamp of last update (millis)
    uint32_t frameCount;                 // Total frames processed for averaging
    uint64_t cumulativeTotalUs;          // Cumulative total for averaging

    ZoneTimingMetrics()
        : zoneBlendUs(0)
        , zoneTotalUs(0)
        , frameSkipCount(0)
        , lastUpdateMs(0)
        , frameCount(0)
        , cumulativeTotalUs(0)
    {
        for (uint8_t i = 0; i < MAX_ZONES; i++) {
            zoneRenderUs[i] = 0;
        }
    }

    /**
     * @brief Get average frame time in milliseconds
     * @return Average frame time, or 0 if no frames processed
     */
    float getAverageFrameMs() const {
        if (frameCount == 0) return 0.0f;
        return static_cast<float>(cumulativeTotalUs) / static_cast<float>(frameCount) / 1000.0f;
    }
};

// ==================== Zone Audio Configuration (Phase 2b.1 + 2b.3) ====================
// Note: AudioBands::BAND_* constants are defined in AudioBandFilter.h

/**
 * @brief Per-zone audio-reactive configuration
 *
 * Enables tempo/beat synchronization and frequency band routing for individual zones.
 *
 * Phase 2b.1: Tempo/beat synchronization
 *   - tempoSync: Enable tempo-locked brightness/speed modulation
 *   - beatModulation: How much beat envelope affects brightness (0-255)
 *   - tempoSpeedScale: How much BPM affects animation speed (0-255)
 *   - beatDecay: Beat pulse decay rate (0-255, higher = faster)
 *
 * Phase 2b.2: Beat-triggered effect transitions
 *   - beatTriggerEnabled: Enable automatic effect cycling on beats
 *   - beatTriggerInterval: Beats between effect changes (1/4/8)
 *   - effectList[]: Up to 8 effects to cycle through
 *
 * Phase 2b.3: Frequency band routing
 *   - audioBand: Which frequency band this zone responds to
 *     - 0 (FULL): All frequencies (default)
 *     - 1 (BASS): 20-250 Hz (kick, bass)
 *     - 2 (MID): 250-2000 Hz (vocals, snare)
 *     - 3 (HIGH): 2000+ Hz (hi-hats, cymbals)
 */
struct ZoneAudioConfig {
    bool tempoSync = false;           ///< Enable tempo synchronization
    uint8_t beatModulation = 0;       ///< 0-255: how much beat affects brightness
    uint8_t tempoSpeedScale = 0;      ///< 0-255: how much BPM affects speed
    uint8_t beatDecay = 128;          ///< Beat pulse decay rate (higher = faster decay)
    uint8_t audioBand = AudioBands::BAND_FULL;  ///< Frequency band filter (0=full, 1=bass, 2=mid, 3=high)

    // Phase 2b.2: Beat-Triggered Effect Cycling
    bool beatTriggerEnabled = false;  ///< Enable beat-triggered effect cycling
    uint8_t beatTriggerInterval = 4;  ///< 1=every beat, 4=every 4 beats, 8=every 8 beats
    uint8_t effectListSize = 0;       ///< Number of effects in rotation list (0-8)
    uint8_t effectList[8] = {0};      ///< Up to 8 effects to cycle through
    uint8_t currentEffectIndex = 0;   ///< Current position in effect list
};

// ==================== Cached Segment Bounds (Phase 2c.3) ====================

/**
 * @brief Pre-computed segment bounds for optimized iteration
 *
 * Phase 2c.3: Caches segment loop bounds to avoid runtime calculation.
 * Recalculated only when layout changes (setLayout/loadPreset).
 *
 * Memory: 16 bytes per zone (64 bytes total for MAX_ZONES=4)
 * Savings: ~30 us per frame (eliminates per-pixel bounds calculation)
 */
struct CachedSegmentBounds {
    // Strip 1 left segment bounds
    uint8_t s1LeftStart;
    uint8_t s1LeftEnd;
    uint8_t s1LeftCount;       // Pre-computed: s1LeftEnd - s1LeftStart + 1

    // Strip 1 right segment bounds
    uint8_t s1RightStart;
    uint8_t s1RightEnd;
    uint8_t s1RightCount;      // Pre-computed: s1RightEnd - s1RightStart + 1

    // Strip 2 offset bounds (Strip 1 indices + 160)
    uint16_t s2LeftStart;      // = s1LeftStart + STRIP_LENGTH
    uint16_t s2RightStart;     // = s1RightStart + STRIP_LENGTH

    // Pre-fetched blend function for this zone (Phase 2c.3)
    BlendFunc blendFunc;
};

// ==================== Zone State ====================

/**
 * @brief Per-zone configuration
 */
struct ZoneState {
    uint8_t effectId;           // Effect to render (0-12 for core effects)
    uint8_t brightness;         // Zone brightness (0-255)
    uint8_t speed;              // Zone speed (1-100)
    uint8_t paletteId;          // Palette ID (0 = use global)
    BlendMode blendMode;        // Compositing mode
    bool enabled;               // Zone enabled flag
    ZoneAudioConfig audio;      // Audio-reactive settings (Phase 2b.1)
};

// ==================== ZoneComposer Class ====================

/**
 * @brief Multi-zone effect orchestrator
 *
 * Manages rendering of multiple effects to different LED segments,
 * compositing them together using blend modes.
 */
class ZoneComposer {
public:
    ZoneComposer();
    ~ZoneComposer() = default;

    // ==================== Initialization ====================

    /**
     * @brief Initialize the zone composer
     * @param renderer Pointer to RendererNode for effect access
     * @return true if initialized successfully
     */
    bool init(RendererNode* renderer);

    // ==================== Rendering ====================

    /**
     * @brief Render all zones and composite to output buffer
     * @param leds Output LED buffer (320 LEDs)
     * @param numLeds Total LED count
     * @param palette Current global palette
     * @param hue Global hue value
     * @param frameCount Current frame number
     * @param deltaTimeMs Time since last frame (ms) - for smooth frame-rate independent animation
     * @param audioCtx Audio context for audio-reactive effects (optional)
     *
     * This is called by RendererNode instead of a single effect.
     */
    void render(CRGB* leds, uint16_t numLeds, CRGBPalette16* palette,
                uint8_t hue, uint32_t frameCount, uint32_t deltaTimeMs,
                const plugins::AudioContext* audioCtx = nullptr);

    // ==================== Zone Control ====================

    /**
     * @brief Enable/disable the zone system
     * @param enabled true to enable multi-zone mode
     */
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    /**
     * @brief Set the zone layout from segment definitions
     * @param segments Array of zone segment definitions
     * @param count Number of zones (must be <= MAX_ZONES)
     * @return true if layout was set successfully, false if validation failed
     */
    bool setLayout(const ZoneSegment* segments, uint8_t count);

    /**
     * @brief Reorder zones according to a new order array
     *
     * Allows reordering zones while maintaining CENTER ORIGIN constraint.
     * Zone 0 MUST always contain LEDs 79/80 (the center point) after reorder.
     *
     * @param newOrder Array of zone IDs in new order (e.g., [2, 0, 1, 3])
     * @param count Number of zones (must match current zone count)
     * @return true if reorder succeeded, false if validation failed
     *
     * Example:
     *   Current: Zone 0=center, Zone 1=middle, Zone 2=outer
     *   newOrder = [2, 0, 1] -> Zone 0=outer, Zone 1=center, Zone 2=middle
     *   This would FAIL because new Zone 0 doesn't contain center LEDs.
     *
     *   newOrder = [0, 2, 1] -> Zone 0=center, Zone 1=outer, Zone 2=middle
     *   This SUCCEEDS because Zone 0 still contains center LEDs 79/80.
     */
    bool reorderZones(const uint8_t* newOrder, uint8_t count);

    uint8_t getZoneCount() const { return m_zoneCount; }
    
    /**
     * @brief Get the current zone segment configuration
     * @return Pointer to zone segment array
     */
    const ZoneSegment* getZoneConfig() const { return m_zoneConfig; }

    // ==================== Per-Zone Settings ====================

    void setZoneEffect(uint8_t zone, uint8_t effectId);
    void setZoneBrightness(uint8_t zone, uint8_t brightness);
    void setZoneSpeed(uint8_t zone, uint8_t speed);
    void setZonePalette(uint8_t zone, uint8_t paletteId);
    void setZoneBlendMode(uint8_t zone, BlendMode mode);
    void setZoneEnabled(uint8_t zone, bool enabled);

    // ==================== Zone Audio Config (Phase 2b.1) ====================

    /**
     * @brief Get zone audio configuration
     * @param zone Zone ID (0-3)
     * @return ZoneAudioConfig for the zone
     */
    ZoneAudioConfig getZoneAudioConfig(uint8_t zone) const;

    /**
     * @brief Set zone audio configuration
     * @param zone Zone ID (0-3)
     * @param config New audio configuration
     */
    void setZoneAudioConfig(uint8_t zone, const ZoneAudioConfig& config);

    /**
     * @brief Set individual audio config fields
     */
    void setZoneTempoSync(uint8_t zone, bool enabled);
    void setZoneBeatModulation(uint8_t zone, uint8_t modulation);
    void setZoneTempoSpeedScale(uint8_t zone, uint8_t scale);
    void setZoneBeatDecay(uint8_t zone, uint8_t decay);
    void setZoneAudioBand(uint8_t zone, uint8_t band);

    // Phase 2b.2: Beat Trigger Configuration
    /**
     * @brief Enable/disable beat-triggered effect cycling for a zone
     * @param zone Zone ID (0-3)
     * @param enabled True to enable beat-triggered cycling
     */
    void setZoneBeatTriggerEnabled(uint8_t zone, bool enabled);

    /**
     * @brief Set beat trigger interval (how many beats between effect changes)
     * @param zone Zone ID (0-3)
     * @param interval 1=every beat, 4=every 4 beats, 8=every 8 beats, etc.
     */
    void setZoneBeatTriggerInterval(uint8_t zone, uint8_t interval);

    /**
     * @brief Set the effect list for beat-triggered cycling
     * @param zone Zone ID (0-3)
     * @param effectIds Array of effect IDs to cycle through
     * @param count Number of effects in the list (max 8)
     */
    void setZoneBeatTriggerEffectList(uint8_t zone, const uint8_t* effectIds, uint8_t count);

    /**
     * @brief Get beat trigger configuration for a zone
     * @param zone Zone ID (0-3)
     * @param enabled Output: enabled state
     * @param interval Output: beat interval
     * @param effectIds Output: effect list (must be at least 8 elements)
     * @param effectCount Output: number of effects in list
     * @param currentIndex Output: current position in effect list
     */
    void getZoneBeatTriggerConfig(uint8_t zone, bool& enabled, uint8_t& interval,
                                   uint8_t* effectIds, uint8_t& effectCount,
                                   uint8_t& currentIndex) const;

    // ==================== State Change Callback ====================

    /**
     * @brief Set callback for zone state changes
     *
     * The callback is invoked whenever any zone property is modified.
     * Throttled internally to max 10 broadcasts per second per zone.
     *
     * @param callback Function to call with the changed zone ID
     */
    void setStateChangeCallback(ZoneStateCallback callback);

private:
    /**
     * @brief Notify listeners of zone state change (throttled)
     *
     * Throttled to max 10 broadcasts per second per zone to prevent
     * WebSocket queue flooding from rapid parameter changes.
     *
     * @param zoneId Zone that changed
     */
    void notifyStateChange(uint8_t zoneId);

public:

    // Getters
    uint8_t getZoneEffect(uint8_t zone) const;
    uint8_t getZoneBrightness(uint8_t zone) const;
    uint8_t getZoneSpeed(uint8_t zone) const;
    uint8_t getZonePalette(uint8_t zone) const;
    BlendMode getZoneBlendMode(uint8_t zone) const;
    bool isZoneEnabled(uint8_t zone) const;

    // ==================== Presets ====================

    /**
     * @brief Load a built-in preset
     * @param presetId Preset number (0-4)
     */
    void loadPreset(uint8_t presetId);

    /**
     * @brief Get preset name
     */
    static const char* getPresetName(uint8_t presetId);

    // ==================== Debug ====================

    void printStatus() const;

    // ==================== Timing Metrics ====================

    /**
     * @brief Get timing metrics for performance monitoring
     * @return Const reference to timing metrics struct
     */
    const ZoneTimingMetrics& getTimingMetrics() const { return m_timing; }

    /**
     * @brief Reset timing metrics to initial state
     */
    void resetTimingMetrics();

    // ==================== Memory Metrics (Phase 2c.2) ====================

    /**
     * @brief Get zone system memory footprint statistics
     *
     * Calculates RAM usage of the zone system including:
     * - Per-zone config storage (ZoneState structs)
     * - LED buffer memory (CRGB buffers per zone)
     * - ZoneComposer class overhead
     * - NVS preset storage estimate
     * - Current ESP heap status
     *
     * @return ZoneMemoryStats with memory footprint data
     */
    ZoneMemoryStats getMemoryStats() const;

private:
    // ==================== Rendering Helpers ====================

    void renderZone(uint8_t zoneId, CRGB* leds, uint16_t numLeds,
                    uint8_t hue, uint32_t frameCount);

    void extractZoneSegment(uint8_t zoneId, const CRGB* source, CRGB* dest);

    void compositeZone(uint8_t zoneId, const CRGB* zoneBuffer);
    
    /**
     * @brief Validate a zone layout configuration
     * @param segments Array of zone segment definitions
     * @param count Number of zones
     * @return true if layout is valid, false otherwise
     */
    bool validateLayout(const ZoneSegment* segments, uint8_t count) const;

    /**
     * @brief Validate and clamp zone ID to safe range [0, MAX_ZONES-1]
     * @param zoneId Zone ID to validate
     * @return Valid zone ID, defaults to 0 if out of bounds
     */
    uint8_t validateZoneId(uint8_t zoneId) const;

    // Phase 2b.2: Beat Trigger Processing
    /**
     * @brief Process beat trigger for a zone on beat tick
     *
     * Called when a beat tick is detected. Increments beat counter
     * and cycles to next effect when interval is reached.
     *
     * @param zoneId Zone to process
     * @param beatTick True if a beat was detected this frame
     */
    void processBeatTrigger(uint8_t zoneId, bool beatTick);

    // Phase 2c.3: Cache Invalidation
    /**
     * @brief Rebuild cached segment bounds for all zones
     *
     * Called when zone layout or blend modes change.
     * Pre-computes loop bounds and fetches blend function pointers.
     */
    void rebuildSegmentCache();

    /**
     * @brief Update cached blend function for a single zone
     *
     * Called when a zone's blend mode changes.
     * More efficient than full cache rebuild for single changes.
     *
     * @param zoneId Zone to update
     */
    void updateBlendFunctionCache(uint8_t zoneId);

    // ==================== Member Variables ====================

    bool m_enabled;                     // Zone system enabled
    bool m_initialized;                 // Init complete flag
    uint8_t m_zoneCount;                // Active zone count
    ZoneSegment m_zoneConfig[MAX_ZONES]; // Runtime storage for zone segment definitions

    ZoneState m_zones[MAX_ZONES];       // Per-zone state

    // Phase 2c.3: Cached segment bounds for optimized iteration
    // Rebuilt on layout change, updated on blend mode change
    CachedSegmentBounds m_cachedBounds[MAX_ZONES];

    RendererNode* m_renderer;          // Renderer for effect access


    // Persistent per-zone render buffers (preserve temporal smoothing/trails)
    // Each zone effect renders into its own full buffer, preventing cross-zone
    // contamination and eliminating strobing caused by buffer resets.
    CRGB m_zoneBuffers[MAX_ZONES][TOTAL_LEDS];

    CRGB m_outputBuffer[TOTAL_LEDS];    // Composited output buffer

    // Reusable per-frame buffers (avoid stack allocations in renderZone)
    CRGBPalette16 m_zonePalette;           // Global palette (used when zone.paletteId == 0)
    CRGBPalette16 m_zonePalettes[MAX_ZONES]; // Per-zone palette storage (for zone-specific palettes)
    plugins::EffectContext m_zoneContext;  // Reused for all zones

    // Monotonic time accumulator for stable getPhase()/time-based animations.
    uint32_t m_totalTimeMs = 0;

    // Performance timing metrics
    ZoneTimingMetrics m_timing;

    // State change callback for WebSocket broadcasting
    ZoneStateCallback m_stateCallback = nullptr;

    // Throttling: track last broadcast time per zone (max 10/sec = 100ms interval)
    uint32_t m_lastBroadcastMs[MAX_ZONES] = {0, 0, 0, 0};
    static constexpr uint32_t BROADCAST_THROTTLE_MS = 100;  // 10 broadcasts/sec max

    // Phase 2b.2: Beat trigger state per zone
    uint8_t m_beatCounter[MAX_ZONES] = {0, 0, 0, 0};  // Current beat count per zone
    bool m_lastBeatTick = false;                       // Previous beat tick state (edge detection)
};

} // namespace zones
} // namespace lightwaveos
