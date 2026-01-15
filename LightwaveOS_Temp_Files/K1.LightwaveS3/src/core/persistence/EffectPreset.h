/**
 * @file EffectPreset.h
 * @brief Effect preset data structure and persistence manager
 *
 * LightwaveOS v2 - Persistence System
 *
 * Manages saving and loading of effect configuration presets to NVS flash.
 * Stores effect ID, brightness, speed, and palette as named presets.
 *
 * Features:
 * - Up to 10 named effect presets
 * - Stores effectId + brightness + speed + paletteId
 * - CRC32 checksum validation
 * - Thread-safe operations via NVSManager
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "NVSManager.h"

namespace lightwaveos {
namespace persistence {

// ==================== Effect Preset Structure ====================

/**
 * @brief Serializable effect preset for NVS storage
 *
 * Stores a complete effect configuration that can be recalled:
 * - Effect ID (which visual effect is active)
 * - Brightness (0-255)
 * - Speed (1-50)
 * - Palette ID (color palette selection)
 */
struct EffectPreset {
    static constexpr uint8_t CURRENT_VERSION = 1;
    static constexpr size_t NAME_MAX_LEN = 32;

    uint8_t version = CURRENT_VERSION;
    char name[NAME_MAX_LEN] = {0};

    // Effect configuration
    uint8_t effectId = 0;       // Effect index (0-96)
    uint8_t brightness = 128;   // Brightness (0-255)
    uint8_t speed = 25;         // Animation speed (1-50)
    uint8_t paletteId = 0;      // Palette index (0-36)

    // Reserved for future use (per-zone configs, etc.)
    uint8_t reserved[4] = {0};

    uint32_t checksum = 0;

    /**
     * @brief Calculate checksum (excludes checksum field itself)
     */
    void calculateChecksum();

    /**
     * @brief Validate checksum
     * @return true if checksum is valid and version matches
     */
    bool isValid() const;

    /**
     * @brief Clamp all values to valid ranges
     */
    void clamp();
};

// ==================== Effect Preset Manager Class ====================

/**
 * @brief Manager for effect preset persistence
 *
 * Provides save/load/delete operations for named effect presets.
 * Uses NVS blob storage with checksum validation.
 *
 * Usage:
 *   auto& mgr = EffectPresetManager::instance();
 *   int8_t id = mgr.savePreset("My Preset", effectId, brightness, speed, paletteId);
 *   mgr.loadPreset(id, effectId, brightness, speed, paletteId);
 */
class EffectPresetManager {
public:
    static constexpr uint8_t MAX_PRESETS = 10;
    static constexpr const char* NVS_NAMESPACE = "effect_pre";

    /**
     * @brief Get the singleton instance
     */
    static EffectPresetManager& instance();

    // Prevent copying
    EffectPresetManager(const EffectPresetManager&) = delete;
    EffectPresetManager& operator=(const EffectPresetManager&) = delete;

    // ==================== Preset Operations ====================

    /**
     * @brief Save current effect config as a new preset
     *
     * @param name Preset name (max 31 chars)
     * @param effectId Effect index
     * @param brightness Brightness value (0-255)
     * @param speed Animation speed (1-50)
     * @param paletteId Palette index
     * @return Preset ID (0-9) on success, -1 on failure
     */
    int8_t savePreset(const char* name,
                      uint8_t effectId,
                      uint8_t brightness,
                      uint8_t speed,
                      uint8_t paletteId);

    /**
     * @brief Save preset to a specific slot (overwrite)
     *
     * @param id Preset slot ID (0-9)
     * @param name Preset name
     * @param effectId Effect index
     * @param brightness Brightness value
     * @param speed Animation speed
     * @param paletteId Palette index
     * @return true on success
     */
    bool savePresetAt(uint8_t id,
                      const char* name,
                      uint8_t effectId,
                      uint8_t brightness,
                      uint8_t speed,
                      uint8_t paletteId);

    /**
     * @brief Load a preset by ID
     *
     * @param id Preset ID (0-9)
     * @param effectId Output: Effect index
     * @param brightness Output: Brightness value
     * @param speed Output: Animation speed
     * @param paletteId Output: Palette index
     * @param nameOut Output: Preset name (optional, provide buffer of NAME_MAX_LEN)
     * @return true on success
     */
    bool loadPreset(uint8_t id,
                    uint8_t& effectId,
                    uint8_t& brightness,
                    uint8_t& speed,
                    uint8_t& paletteId,
                    char* nameOut = nullptr);

    /**
     * @brief Get full preset data by ID
     *
     * @param id Preset ID (0-9)
     * @param preset Output: Full preset structure
     * @return true on success
     */
    bool getPreset(uint8_t id, EffectPreset& preset);

    /**
     * @brief Delete a preset by ID
     *
     * @param id Preset ID (0-9)
     * @return true on success
     */
    bool deletePreset(uint8_t id);

    /**
     * @brief List all saved presets
     *
     * @param names Output: Array of preset names (provide MAX_PRESETS elements)
     * @param ids Output: Array of preset IDs (provide MAX_PRESETS elements)
     * @return Number of presets found
     */
    uint8_t listPresets(char names[][EffectPreset::NAME_MAX_LEN], uint8_t* ids);

    /**
     * @brief Check if a preset exists
     *
     * @param id Preset ID (0-9)
     * @return true if preset exists and is valid
     */
    bool hasPreset(uint8_t id) const;

    /**
     * @brief Get preset count
     *
     * @return Number of saved presets
     */
    uint8_t getPresetCount() const;

    /**
     * @brief Find next available preset slot
     *
     * @return Preset ID (0-9) or -1 if all slots used
     */
    int8_t findFreeSlot() const;

private:
    EffectPresetManager() = default;

    // NVS key format: "preset_0" through "preset_9"
    static void makeKey(uint8_t id, char* key);
};

} // namespace persistence
} // namespace lightwaveos
