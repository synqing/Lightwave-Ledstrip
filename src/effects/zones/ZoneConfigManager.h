#ifndef ZONE_CONFIG_MANAGER_H
#define ZONE_CONFIG_MANAGER_H

#include "ZoneConfig.h"
#include <nvs.h>
#include <nvs_flash.h>

// Forward declaration
class ZoneComposer;

// Zone Configuration Manager - Handles NVS persistence and presets
class ZoneConfigManager {
public:
    ZoneConfigManager(ZoneComposer* composer);

    // NVS operations
    bool saveToNVS();
    bool loadFromNVS();

    // Preset management
    bool loadPreset(uint8_t presetId);  // 0-4
    const char* getPresetName(uint8_t presetId) const;
    uint8_t getPresetCount() const { return ZONE_PRESET_COUNT; }

    // Configuration export/import
    void exportConfig(ZoneConfig& config);
    void importConfig(const ZoneConfig& config);

private:
    ZoneComposer* m_composer;  // Reference to zone composer

    static constexpr const char* NVS_NAMESPACE = "zone_config";
    static constexpr const char* NVS_KEY_CONFIG = "config";

    bool validateConfig(const ZoneConfig& config) const;
};

#endif // ZONE_CONFIG_MANAGER_H
