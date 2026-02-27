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
 */

#pragma once

#include <FastLED.h>
#include <functional>
#include "ZoneDefinition.h"
#include "BlendMode.h"
#include "../../config/effect_ids.h"
#include "../../core/actors/RendererActor.h"
#include "../../plugins/api/EffectContext.h"

// Forward declaration for audio context
namespace lightwaveos { namespace plugins { struct AudioContext; } }

namespace lightwaveos {
namespace zones {

using namespace lightwaveos::actors;

// Use the EffectRenderFn typedef from RendererActor
using EffectFunc = EffectRenderFn;

// ==================== Zone Audio Config ====================

/**
 * @brief Per-zone audio-reactive configuration
 */
struct ZoneAudioConfig {
    bool tempoSync = false;           // Sync effect speed to detected tempo
    bool beatModulation = false;      // Modulate effect with beat phase
    float tempoSpeedScale = 1.0f;     // Speed multiplier when tempo sync is active
    float beatDecay = 0.5f;           // Beat modulation decay rate (0.0-1.0)
    uint8_t audioBand = 0;            // Primary audio band for this zone (0-7)
    bool beatTriggerEnabled = false;  // Enable beat-triggered transitions
    uint16_t beatTriggerInterval = 500; // Minimum interval between beat triggers (ms)
};

// ==================== Zone State ====================

/**
 * @brief Per-zone configuration
 */
struct ZoneState {
    EffectId effectId;           // Effect to render (stable namespaced ID)
    uint8_t brightness;         // Zone brightness (0-255)
    uint8_t speed;              // Zone speed (1-100)
    uint8_t paletteId;          // Palette ID (0 = use global)
    BlendMode blendMode;        // Compositing mode
    bool enabled;               // Zone enabled flag
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
     * @param renderer Pointer to RendererActor for effect access
     * @return true if initialized successfully
     */
    bool init(RendererActor* renderer);

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
     * This is called by RendererActor instead of a single effect.
     */
    void render(CRGB* leds, uint16_t numLeds, CRGBPalette16* palette,
                uint8_t hue, uint32_t frameCount, uint32_t deltaTimeMs,
                const plugins::AudioContext* audioCtx = nullptr);

    // ==================== Zone Control ====================

    /**
     * @brief Enable/disable the zone system
     * @param enabled true to enable multi-zone mode
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    /**
     * @brief Set the zone layout from segment definitions
     * @param segments Array of zone segment definitions
     * @param count Number of zones (must be <= MAX_ZONES)
     * @return true if layout was set successfully, false if validation failed
     */
    bool setLayout(const ZoneSegment* segments, uint8_t count);
    
    uint8_t getZoneCount() const { return m_zoneCount; }
    
    /**
     * @brief Get the current zone segment configuration
     * @return Pointer to zone segment array
     */
    const ZoneSegment* getZoneConfig() const { return m_zoneConfig; }

    // ==================== Per-Zone Settings ====================

    void setZoneEffect(uint8_t zone, EffectId effectId);
    void setZoneBrightness(uint8_t zone, uint8_t brightness);
    void setZoneSpeed(uint8_t zone, uint8_t speed);
    void setZonePalette(uint8_t zone, uint8_t paletteId);
    void setZoneBlendMode(uint8_t zone, BlendMode mode);
    void setZoneEnabled(uint8_t zone, bool enabled);

    // Getters
    EffectId getZoneEffect(uint8_t zone) const;
    uint8_t getZoneBrightness(uint8_t zone) const;
    uint8_t getZoneSpeed(uint8_t zone) const;
    uint8_t getZonePalette(uint8_t zone) const;
    BlendMode getZoneBlendMode(uint8_t zone) const;
    bool isZoneEnabled(uint8_t zone) const;

    // ==================== Zone Audio Config ====================

    /**
     * @brief Get audio configuration for a zone
     * @param zone Zone ID (0-3)
     * @return ZoneAudioConfig for the zone (default config if zone invalid)
     */
    ZoneAudioConfig getZoneAudioConfig(uint8_t zone) const;

    /**
     * @brief Set audio configuration for a zone
     * @param zone Zone ID (0-3)
     * @param config Audio configuration to apply
     */
    void setZoneAudioConfig(uint8_t zone, const ZoneAudioConfig& config);

    // ==================== State Change Callback ====================

    /**
     * @brief Set callback for zone state changes
     * @param callback Function called when zone state changes, receives zone ID
     */
    void setStateChangeCallback(std::function<void(uint8_t)> callback);

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

private:
    // ==================== Rendering Helpers ====================

    void renderZone(uint8_t zoneId, CRGB* leds, uint16_t numLeds,
                    uint8_t hue, uint32_t frameCount, float deltaSeconds);

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

    // ==================== Member Variables ====================

    bool m_enabled;                     // Zone system enabled
    bool m_initialized;                 // Init complete flag
    uint8_t m_zoneCount;                // Active zone count
    ZoneSegment m_zoneConfig[MAX_ZONES]; // Runtime storage for zone segment definitions

    ZoneState m_zones[MAX_ZONES];       // Per-zone state
    ZoneAudioConfig m_zoneAudioConfigs[MAX_ZONES]; // Per-zone audio configuration

    std::function<void(uint8_t)> m_stateChangeCallback; // Callback for zone state changes

    RendererActor* m_renderer;          // Renderer for effect access

    // Persistent per-zone render buffers (preserve temporal smoothing/trails)
    // Each zone effect renders into its own full buffer, preventing cross-zone
    // contamination and eliminating strobing caused by buffer resets.
    // Allocated during init() (prefer PSRAM). Layout: MAX_ZONES contiguous buffers
    // of TOTAL_LEDS each (zone 0 first).
    CRGB* m_zoneBuffers = nullptr;

    // Composited output buffer (allocated during init()).
    CRGB* m_outputBuffer = nullptr;

    // Reusable per-frame buffers (avoid stack allocations in renderZone)
    CRGBPalette16 m_zonePalette;           // Global palette (used when zone.paletteId == 0)
    CRGBPalette16 m_zonePalettes[MAX_ZONES]; // Per-zone palette storage (for zone-specific palettes)
    plugins::EffectContext m_zoneContext;  // Reused for all zones

    // Per-zone time accumulators for stable slow-motion scaling.
    float m_zoneTimeSeconds[MAX_ZONES];
    float m_zoneTimeSecondsRaw[MAX_ZONES];
    float m_zoneFrameAccumulator[MAX_ZONES];
    uint32_t m_zoneFrameCount[MAX_ZONES];
};

} // namespace zones
} // namespace lightwaveos
