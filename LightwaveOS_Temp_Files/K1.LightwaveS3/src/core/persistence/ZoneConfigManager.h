/**
 * @file ZoneConfigManager.h
 * @brief Zone-specific persistence manager for NVS storage
 *
 * LightwaveOS v2 - Persistence System
 *
 * Manages saving and loading of ZoneComposer configuration to NVS flash.
 * Includes checksum validation, preset management, and system state persistence.
 *
 * Features:
 * - Zone configuration persistence (all 4 zones)
 * - System state persistence (effect, brightness, speed, palette)
 * - 5 built-in presets
 * - CRC32 checksum validation
 * - Graceful first-boot handling
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>
#include "NVSManager.h"
#include "../../effects/zones/ZoneComposer.h"

namespace lightwaveos {
namespace persistence {

using namespace lightwaveos::zones;

// ==================== Zone Configuration Structure ====================

/**
 * @brief Serializable zone configuration for NVS storage
 *
 * This structure mirrors ZoneState from ZoneComposer but with fixed-size
 * arrays suitable for blob storage.
 */
struct ZoneConfigData {
    // Header for version compatibility
    uint8_t version;                    // Config format version (2 for segment-based)

    // Zone system state
    ZoneSegment segments[MAX_ZONES];    // Zone segment definitions
    uint8_t zoneCount;                   // Number of active zones
    bool systemEnabled;                  // Global zone system enable

    // Per-zone settings (4 zones max)
    uint8_t zoneEffects[MAX_ZONES];     // Effect ID per zone
    bool zoneEnabled[MAX_ZONES];        // Enable flag per zone
    uint8_t zoneBrightness[MAX_ZONES];  // Brightness per zone (0-255)
    uint8_t zoneSpeed[MAX_ZONES];       // Speed per zone (1-50)
    uint8_t zonePalette[MAX_ZONES];     // Palette ID per zone (0=global)
    uint8_t zoneBlendMode[MAX_ZONES];   // Blend mode per zone

    // Audio config per zone (v3)
    bool zoneTempoSync[MAX_ZONES];         // Tempo synchronization enabled
    uint8_t zoneBeatModulation[MAX_ZONES]; // Beat modulation amount (0-255)
    uint8_t zoneTempoSpeedScale[MAX_ZONES];// Tempo speed scale (0-200)
    uint8_t zoneBeatDecay[MAX_ZONES];      // Beat decay rate (0-255)
    uint8_t zoneAudioBand[MAX_ZONES];      // Audio band filter (0-3)

    // Beat trigger config per zone (v3)
    bool zoneBeatTriggerEnabled[MAX_ZONES];   // Beat trigger enabled
    uint8_t zoneBeatTriggerInterval[MAX_ZONES]; // Beat interval (1,2,4,8)
    uint8_t zoneEffectListSize[MAX_ZONES];    // Effect rotation list size (0-8)
    uint8_t zoneEffectList[MAX_ZONES][8];     // Effect rotation lists

    // Checksum for data integrity
    uint32_t checksum;

    // Calculate checksum (excludes checksum field itself)
    void calculateChecksum();

    // Validate checksum
    bool isValid() const;
};

// ==================== System Configuration Structure ====================

/**
 * @brief Global system state for NVS storage
 *
 * Stores the non-zone-specific settings that persist across reboots.
 */
struct SystemConfigData {
    uint8_t version;                    // Config format version (currently 1)

    uint8_t effectId;                   // Current effect ID
    uint8_t brightness;                 // Global brightness (0-255)
    uint8_t speed;                      // Animation speed (1-50)
    uint8_t paletteId;                  // Current palette ID

    uint32_t checksum;

    void calculateChecksum();
    bool isValid() const;
};

// ==================== Preset Definition ====================

/**
 * @brief Built-in zone preset
 */
struct ZonePreset {
    const char* name;
    ZoneConfigData config;
};

// Number of built-in presets
constexpr uint8_t ZONE_PRESET_COUNT = 5;

// Preset declarations (defined in .cpp)
extern const ZonePreset ZONE_PRESETS[ZONE_PRESET_COUNT];

// ==================== ZoneConfigManager Class ====================

/**
 * @brief Manages zone configuration persistence to NVS
 *
 * Usage:
 *   ZoneConfigManager configMgr(&zoneComposer);
 *
 *   // Load saved config on boot
 *   if (!configMgr.loadFromNVS()) {
 *       configMgr.loadPreset(0);  // Load default preset
 *   }
 *
 *   // Save config when changed
 *   configMgr.saveToNVS();
 *
 *   // Load a preset
 *   configMgr.loadPreset(2);  // Triple Rings
 */
class ZoneConfigManager {
public:
    /**
     * @brief Construct with reference to ZoneComposer
     * @param composer Pointer to the ZoneComposer instance
     */
    explicit ZoneConfigManager(ZoneComposer* composer);

    // ==================== NVS Operations ====================

    /**
     * @brief Save current zone configuration to NVS
     * @return true if saved successfully
     */
    bool saveToNVS();

    /**
     * @brief Load zone configuration from NVS
     * @return true if loaded successfully (false if no saved config or invalid)
     */
    bool loadFromNVS();

    // ==================== System State Operations ====================

    /**
     * @brief Save system state (effect, brightness, speed, palette) to NVS
     * @param effectId Current effect ID
     * @param brightness Current brightness
     * @param speed Current speed
     * @param paletteId Current palette ID
     * @return true if saved successfully
     */
    bool saveSystemState(uint8_t effectId, uint8_t brightness, uint8_t speed, uint8_t paletteId);

    /**
     * @brief Load system state from NVS
     * @param effectId Output: loaded effect ID
     * @param brightness Output: loaded brightness
     * @param speed Output: loaded speed
     * @param paletteId Output: loaded palette ID
     * @return true if loaded successfully
     */
    bool loadSystemState(uint8_t& effectId, uint8_t& brightness, uint8_t& speed, uint8_t& paletteId);

    // ==================== Preset Management ====================

    /**
     * @brief Load a built-in preset
     * @param presetId Preset ID (0-4)
     * @return true if preset loaded successfully
     */
    bool loadPreset(uint8_t presetId);

    /**
     * @brief Get preset name
     * @param presetId Preset ID (0-4)
     * @return Preset name or "Invalid" if out of range
     */
    static const char* getPresetName(uint8_t presetId);

    /**
     * @brief Get number of available presets
     */
    static uint8_t getPresetCount() { return ZONE_PRESET_COUNT; }

    // ==================== Config Export/Import ====================

    /**
     * @brief Export current ZoneComposer state to config structure
     * @param config Output: configuration data
     */
    void exportConfig(ZoneConfigData& config);

    /**
     * @brief Import configuration to ZoneComposer
     * @param config Configuration data to apply
     */
    void importConfig(const ZoneConfigData& config);

    // ==================== Validation ====================

    /**
     * @brief Validate configuration values are within acceptable ranges
     * @param config Configuration to validate
     * @return true if all values are valid
     */
    bool validateConfig(const ZoneConfigData& config) const;

    /**
     * @brief Get the last error from load/save operations
     */
    NVSResult getLastError() const { return m_lastError; }

private:
    ZoneComposer* m_composer;       // Reference to zone composer
    NVSResult m_lastError;          // Last operation result

    // NVS namespace and keys
    static constexpr const char* NVS_NAMESPACE = "zone_config";
    static constexpr const char* NVS_KEY_ZONES = "zones";
    static constexpr const char* NVS_NS_SYSTEM = "system_cfg";
    static constexpr const char* NVS_KEY_STATE = "state";

    // Config version for future compatibility
    static constexpr uint8_t CONFIG_VERSION = 3;  // v3: Added audio config fields

    // Effect limits (should match RendererActor upper bound)
    static constexpr uint8_t MAX_EFFECT_ID = 96;
    static constexpr uint8_t MIN_SPEED = 1;
    static constexpr uint8_t MAX_SPEED = 100;
    // Palette validation: use palettes::validatePaletteId() or MASTER_PALETTE_COUNT (75 palettes: 0-74)
};

} // namespace persistence
} // namespace lightwaveos
