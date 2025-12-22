#ifndef ZONE_CONFIG_H
#define ZONE_CONFIG_H

#include <stdint.h>
#include <string.h>  // For memset, strncpy in UserPreset

// Zone configuration structure for NVS persistence
struct ZoneConfig {
    uint8_t zoneCount;                          // 1-4 active zones
    uint8_t zoneEffects[4];                     // Effect ID per zone (0-46)
    bool zoneEnabled[4];                        // Enable/disable per zone
    uint8_t zoneBrightness[4];                  // Per-zone brightness (0-255)
    uint8_t zoneSpeed[4];                       // Per-zone speed (1-50)
    uint8_t zonePalette[4];                     // Per-zone palette (0=global, 1-36=specific)
    uint8_t zoneBlendMode[4];                   // Per-zone blend mode (0=overwrite, 1=add, etc.)
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

// ============================================================================
// User Preset System (Phase C.1)
// ============================================================================

// Maximum number of user-saveable presets
#define MAX_USER_PRESETS 8

// Maximum length of user preset name (including null terminator)
#define USER_PRESET_NAME_LEN 16

/**
 * User-defined preset structure for NVS storage
 *
 * Unlike built-in ZonePreset (which uses const char* in flash),
 * UserPreset stores the name in a mutable char array for NVS persistence.
 *
 * Storage size: ~50 bytes per preset (name + config + checksum)
 * Total for 8 presets: ~400 bytes in NVS
 */
struct UserPreset {
    char name[USER_PRESET_NAME_LEN];    // User-provided name (15 chars + null)
    ZoneConfig config;                   // Zone configuration (~30 bytes)
    uint16_t checksum;                   // Validation checksum

    // Calculate checksum for validation
    void calculateChecksum();

    // Validate checksum matches stored data
    bool isValid() const;

    // Check if preset slot is empty (no name set)
    bool isEmpty() const { return name[0] == '\0'; }

    // Clear preset data
    void clear() {
        memset(name, 0, USER_PRESET_NAME_LEN);
        memset(&config, 0, sizeof(ZoneConfig));
        checksum = 0;
    }

    // Set name with truncation safety
    void setName(const char* newName) {
        if (newName) {
            strncpy(name, newName, USER_PRESET_NAME_LEN - 1);
            name[USER_PRESET_NAME_LEN - 1] = '\0';
        }
    }
};

#endif // ZONE_CONFIG_H
