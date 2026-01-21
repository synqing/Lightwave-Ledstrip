/**
 * @file ZonePreset.h
 * @brief Zone preset data structure and persistence manager
 *
 * LightwaveOS v2 - Persistence System
 *
 * Manages saving and loading of complete zone configuration presets to NVS flash.
 * Stores zone count, per-zone settings (effect, brightness, speed, palette, blend mode),
 * and layout segment definitions.
 *
 * Features:
 * - Up to 5 named zone presets
 * - Stores complete zone configuration state
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

// Forward declarations
namespace lightwaveos {
namespace zones {
class ZoneComposer;
}
namespace nodes {
class NodeOrchestrator;
}
}

namespace lightwaveos {
namespace persistence {

// ==================== Zone Preset Structure ====================

/**
 * @brief Serializable zone preset for NVS storage
 *
 * Stores a complete zone configuration that can be recalled:
 * - Zone system enabled state
 * - Zone count (1-4)
 * - Per-zone configuration (effect, brightness, speed, palette, blend mode)
 * - Layout segment definitions
 */
struct ZonePreset {
    static constexpr uint8_t CURRENT_VERSION = 2;  // v2: Added audio config fields
    static constexpr size_t NAME_MAX_LEN = 32;
    static constexpr uint8_t MAX_ZONES = 4;

    uint8_t version = CURRENT_VERSION;
    char name[NAME_MAX_LEN] = {0};

    bool zonesEnabled = false;
    uint8_t zoneCount = 1;

    // Per-zone config
    struct ZoneConfig {
        bool enabled = true;
        uint8_t effectId = 0;
        uint8_t brightness = 255;
        uint8_t speed = 25;
        uint8_t paletteId = 0;
        uint8_t blendMode = 0;  // Maps to BlendMode enum

        // Audio config (v2)
        bool tempoSync = false;
        uint8_t beatModulation = 128;   // 0-255, how much tempo affects speed
        uint8_t tempoSpeedScale = 100;  // 0-200, tempo multiplier percentage
        uint8_t beatDecay = 200;        // 0-255, how fast beat energy fades
        uint8_t audioBand = 0;          // 0=Full, 1=Bass, 2=Mid, 3=High

        // Beat trigger config (v2)
        bool beatTriggerEnabled = false;
        uint8_t beatTriggerInterval = 4;  // Beats between effect changes (1,2,4,8)
        uint8_t effectListSize = 0;       // Number of effects in rotation
        uint8_t effectList[8] = {0};      // Up to 8 effects for rotation
    } zones[MAX_ZONES];

    // Layout segments (zone boundaries on the LED strip)
    struct Segment {
        uint8_t s1LeftStart = 0;
        uint8_t s1LeftEnd = 79;
        uint8_t s1RightStart = 80;
        uint8_t s1RightEnd = 159;
    } segments[MAX_ZONES];

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

// ==================== Zone Preset Manager Class ====================

/**
 * @brief Manager for zone preset persistence
 *
 * Provides save/load/delete operations for named zone presets.
 * Uses NVS blob storage with checksum validation.
 *
 * Usage:
 *   auto& mgr = ZonePresetManager::instance();
 *   int8_t id = mgr.savePreset("My Zone Config", zoneComposer);
 *   mgr.applyPreset(id, zoneComposer, orchestrator);
 */
class ZonePresetManager {
public:
    static constexpr uint8_t MAX_PRESETS = 5;  // Limit for NVS space
    static constexpr const char* NVS_NAMESPACE = "zone_pre";

    /**
     * @brief Get the singleton instance
     */
    static ZonePresetManager& instance();

    // Prevent copying
    ZonePresetManager(const ZonePresetManager&) = delete;
    ZonePresetManager& operator=(const ZonePresetManager&) = delete;

    // ==================== Preset Operations ====================

    /**
     * @brief Save current zone config as a new preset
     *
     * @param name Preset name (max 31 chars)
     * @param composer ZoneComposer to capture state from
     * @return Preset ID (0-4) on success, -1 on failure
     */
    int8_t savePreset(const char* name, const zones::ZoneComposer* composer);

    /**
     * @brief Save preset to a specific slot (overwrite)
     *
     * @param id Preset slot ID (0-4)
     * @param name Preset name
     * @param composer ZoneComposer to capture state from
     * @return true on success
     */
    bool savePresetAt(uint8_t id, const char* name, const zones::ZoneComposer* composer);

    /**
     * @brief Apply a preset by ID
     *
     * @param id Preset ID (0-4)
     * @param composer ZoneComposer to apply state to
     * @param orchestrator NodeOrchestrator for setting global effect if single zone
     * @return true on success
     */
    bool applyPreset(uint8_t id, zones::ZoneComposer* composer, nodes::NodeOrchestrator& orchestrator);

    /**
     * @brief Get full preset data by ID
     *
     * @param id Preset ID (0-4)
     * @param preset Output: Full preset structure
     * @return true on success
     */
    bool getPreset(uint8_t id, ZonePreset& preset);

    /**
     * @brief Delete a preset by ID
     *
     * @param id Preset ID (0-4)
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
    uint8_t listPresets(char names[][ZonePreset::NAME_MAX_LEN], uint8_t* ids);

    /**
     * @brief Check if a preset exists
     *
     * @param id Preset ID (0-4)
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
     * @return Preset ID (0-4) or -1 if all slots used
     */
    int8_t findFreeSlot() const;

private:
    ZonePresetManager() = default;

    // NVS key format: "zpreset_0" through "zpreset_4"
    static void makeKey(uint8_t id, char* key);

    // Helper to capture zone state from composer
    static void captureFromComposer(ZonePreset& preset, const zones::ZoneComposer* composer);
};

} // namespace persistence
} // namespace lightwaveos
