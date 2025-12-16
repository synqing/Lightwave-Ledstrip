#ifndef ZONE_COMPOSER_H
#define ZONE_COMPOSER_H

#include <FastLED.h>
#include "ZoneDefinition.h"
#include "ZoneConfigManager.h"
#include "../../effects.h"
#include "../../config/hardware_config.h"

// Forward declaration
class ZoneConfigManager;

// Zone Composer - Manages multiple effects across different LED zones
// Uses buffer proxy pattern to work with existing parameterless effects
class ZoneComposer {
public:
    ZoneComposer();

    // Zone management
    void setZoneEffect(uint8_t zoneId, uint8_t effectId);
    void enableZone(uint8_t zoneId, bool enabled);
    void setZoneCount(uint8_t count);
    uint8_t getZoneCount() const { return m_zoneCount; }

    // Per-zone brightness control (0-255)
    void setZoneBrightness(uint8_t zoneId, uint8_t brightness);
    uint8_t getZoneBrightness(uint8_t zoneId) const;

    // Per-zone speed control (1-50)
    void setZoneSpeed(uint8_t zoneId, uint8_t speed);
    uint8_t getZoneSpeed(uint8_t zoneId) const;

    // Per-zone palette control (0=global, 1-36=specific)
    void setZonePalette(uint8_t zoneId, uint8_t paletteId);
    uint8_t getZonePalette(uint8_t zoneId) const;

    // Zone queries (for config export)
    uint8_t getZoneEffect(uint8_t zoneId) const;
    bool isZoneEnabled(uint8_t zoneId) const;

    // Zone system control
    void enable();
    void disable();
    bool isEnabled() const { return m_enabled; }

    // Configuration management
    bool saveConfig();
    bool loadConfig();
    bool loadPreset(uint8_t presetId);
    const char* getPresetName(uint8_t presetId) const;

    // Main rendering
    void render();

    // Debug
    void printStatus() const;

private:
    // Zone configuration
    uint8_t m_zoneCount;          // Number of active zones (1-4)
    uint8_t m_zoneEffects[HardwareConfig::MAX_ZONES];  // Effect ID for each zone
    bool m_zoneEnabled[HardwareConfig::MAX_ZONES];     // Enable/disable per zone
    uint8_t m_zoneBrightness[HardwareConfig::MAX_ZONES];  // Per-zone brightness (0-255)
    uint8_t m_zoneSpeed[HardwareConfig::MAX_ZONES];       // Per-zone speed (1-50)
    uint8_t m_zonePalette[HardwareConfig::MAX_ZONES];    // Per-zone palette (0=global)
    bool m_enabled;                     // Global zone system enable

    // Dynamic zone configuration selection
    const ZoneDefinition* m_activeConfig;  // Pointer to active config array (3-zone or 4-zone)
    uint8_t m_configSize;                   // Size of active config (3 or 4)

    // Configuration manager
    ZoneConfigManager* m_configManager;

    // Temporary rendering buffers (40 LEDs max per zone)
    CRGB m_tempStrip1[40];
    CRGB m_tempStrip2[40];

    // Output composite buffers
    CRGB m_outputStrip1[160];
    CRGB m_outputStrip2[160];

    // Private rendering methods
    void renderZone(uint8_t zoneId);
    void mapZoneToOutput(uint8_t zoneId);
    void clearOutputBuffers();
    void copyOutputToMain();
};

#endif // ZONE_COMPOSER_H
