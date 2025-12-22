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

    // Built-in preset management
    bool loadPreset(uint8_t presetId);  // 0-4
    const char* getPresetName(uint8_t presetId) const;
    uint8_t getPresetCount() const { return ZONE_PRESET_COUNT; }

    // ========================================================================
    // User Preset Management (Phase C.1)
    // ========================================================================

    /**
     * Save current zone configuration to a user preset slot
     * @param slot Preset slot (0 to MAX_USER_PRESETS-1)
     * @param name User-provided name (max 15 chars, will be truncated)
     * @return true if saved successfully
     */
    bool saveUserPreset(uint8_t slot, const char* name);

    /**
     * Load a user preset and apply it to the zone composer
     * @param slot Preset slot (0 to MAX_USER_PRESETS-1)
     * @return true if loaded successfully
     */
    bool loadUserPreset(uint8_t slot);

    /**
     * Delete a user preset from NVS
     * @param slot Preset slot (0 to MAX_USER_PRESETS-1)
     * @return true if deleted successfully
     */
    bool deleteUserPreset(uint8_t slot);

    /**
     * Check if a user preset slot has saved data
     * @param slot Preset slot (0 to MAX_USER_PRESETS-1)
     * @return true if slot contains a valid preset
     */
    bool hasUserPreset(uint8_t slot) const;

    /**
     * Get user preset data without applying it
     * @param slot Preset slot (0 to MAX_USER_PRESETS-1)
     * @param preset Output: preset data if found
     * @return true if preset exists and is valid
     */
    bool getUserPreset(uint8_t slot, UserPreset& preset) const;

    /**
     * Get the name of a user preset
     * @param slot Preset slot (0 to MAX_USER_PRESETS-1)
     * @param name Output buffer for name
     * @param maxLen Size of output buffer
     * @return true if preset exists
     */
    bool getUserPresetName(uint8_t slot, char* name, size_t maxLen) const;

    /**
     * Count how many user preset slots are filled
     * @return Number of saved user presets (0 to MAX_USER_PRESETS)
     */
    uint8_t getFilledUserPresetCount() const;

    // Configuration export/import
    void exportConfig(ZoneConfig& config);
    void importConfig(const ZoneConfig& config);

private:
    ZoneComposer* m_composer;  // Reference to zone composer

    static constexpr const char* NVS_NAMESPACE = "zone_config";
    static constexpr const char* NVS_KEY_CONFIG = "config";
    static constexpr const char* NVS_USER_PRESET_NAMESPACE = "user_presets";

    bool validateConfig(const ZoneConfig& config) const;

    // Generate NVS key for user preset slot (e.g., "preset_0")
    void getUserPresetKey(uint8_t slot, char* key, size_t keyLen) const;
};

#endif // ZONE_CONFIG_MANAGER_H
