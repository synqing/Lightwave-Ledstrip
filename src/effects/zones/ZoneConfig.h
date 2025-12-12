#ifndef ZONE_CONFIG_H
#define ZONE_CONFIG_H

#include <stdint.h>

// Zone configuration structure for NVS persistence
struct ZoneConfig {
    uint8_t zoneCount;                          // 1-4 active zones
    uint8_t zoneEffects[4];                     // Effect ID per zone (0-46)
    bool zoneEnabled[4];                        // Enable/disable per zone
    uint8_t zoneBrightness[4];                  // Per-zone brightness (0-255)
    uint8_t zoneSpeed[4];                       // Per-zone speed (1-50)
    bool systemEnabled;                         // Global zone system enable
    uint16_t checksum;                          // Data validation

    // Calculate checksum for validation
    void calculateChecksum();

    // Validate checksum
    bool isValid() const;
};

// Preset definition structure
struct ZonePreset {
    const char* name;
    ZoneConfig config;
};

// Number of predefined presets
#define ZONE_PRESET_COUNT 5

// Predefined zone presets
extern const ZonePreset ZONE_PRESETS[ZONE_PRESET_COUNT];

#endif // ZONE_CONFIG_H
