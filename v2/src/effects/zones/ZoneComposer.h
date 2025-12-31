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
#include "ZoneDefinition.h"
#include "BlendMode.h"
#include "../../core/actors/RendererActor.h"

namespace lightwaveos {
namespace zones {

using namespace lightwaveos::actors;

// Use the EffectRenderFn typedef from RendererActor
using EffectFunc = EffectRenderFn;

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
     *
     * This is called by RendererActor instead of a single effect.
     */
    void render(CRGB* leds, uint16_t numLeds, CRGBPalette16* palette,
                uint8_t hue, uint32_t frameCount);

    // ==================== Zone Control ====================

    /**
     * @brief Enable/disable the zone system
     * @param enabled true to enable multi-zone mode
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    /**
     * @brief Set the zone layout (3 or 4 zones)
     * @param layout Zone layout type
     */
    void setLayout(ZoneLayout layout);
    ZoneLayout getLayout() const { return m_layout; }
    uint8_t getZoneCount() const { return m_zoneCount; }

    // ==================== Per-Zone Settings ====================

    void setZoneEffect(uint8_t zone, uint8_t effectId);
    void setZoneBrightness(uint8_t zone, uint8_t brightness);
    void setZoneSpeed(uint8_t zone, uint8_t speed);
    void setZonePalette(uint8_t zone, uint8_t paletteId);
    void setZoneBlendMode(uint8_t zone, BlendMode mode);
    void setZoneEnabled(uint8_t zone, bool enabled);

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

private:
    // ==================== Rendering Helpers ====================

    void renderZone(uint8_t zoneId, CRGB* leds, uint16_t numLeds,
                    CRGBPalette16* palette, uint8_t hue, uint32_t frameCount);

    void extractZoneSegment(uint8_t zoneId, const CRGB* source, CRGB* dest);

    void compositeZone(uint8_t zoneId, const CRGB* zoneBuffer);

    // ==================== Member Variables ====================

    bool m_enabled;                     // Zone system enabled
    bool m_initialized;                 // Init complete flag
    ZoneLayout m_layout;                // Current layout (3 or 4 zones)
    uint8_t m_zoneCount;                // Active zone count
    const ZoneSegment* m_zoneConfig;    // Pointer to active zone definitions

    ZoneState m_zones[MAX_ZONES];       // Per-zone state

    RendererActor* m_renderer;          // Renderer for effect access

    // Temp buffers for zone rendering
    CRGB m_tempBuffer[TOTAL_LEDS];      // Full temp buffer for effect render
    CRGB m_outputBuffer[TOTAL_LEDS];    // Composited output buffer
};

} // namespace zones
} // namespace lightwaveos
