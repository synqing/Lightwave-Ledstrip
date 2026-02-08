/**
 * @file ZonePresetManager.h
 * @brief User-saveable zone preset persistence for NVS storage
 *
 * LightwaveOS v2 - Persistence System
 *
 * Manages saving and loading of user-defined zone presets to NVS flash.
 * Each preset stores a complete multi-zone configuration snapshot including
 * zone count, per-zone effects, palettes, brightness, speed, blend modes,
 * and segment definitions.
 *
 * Features:
 * - 16 user preset slots (0-15)
 * - Complete zone layout persistence (segments + per-zone settings)
 * - CRC32 checksum validation
 * - Direct ZoneComposer integration for save/load workflows
 * - Preset metadata listing for UI display
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>
#include "NVSManager.h"
#include "../../config/limits.h"  // Single source of truth for system limits

// Forward declaration to avoid circular includes
namespace lightwaveos {
namespace zones {
    class ZoneComposer;
}
}

namespace lightwaveos {
namespace persistence {

// ==================== Constants ====================

constexpr uint8_t ZONE_PRESET_MAX_SLOTS = 16;
constexpr uint8_t ZONE_PRESET_MAX_ZONES = 4;
constexpr uint8_t ZONE_PRESET_NAME_LENGTH = 32;

// ==================== Zone Preset Entry ====================

/**
 * @brief Per-zone configuration within a preset
 *
 * Stores all settings for a single zone including effect, palette,
 * brightness, speed, blend mode, and LED segment ranges.
 */
struct ZonePresetEntry {
    uint8_t effectId;           // Effect ID (0-255)
    uint8_t paletteId;          // Palette ID (0-74)
    uint8_t brightness;         // Zone brightness (0-255)
    uint8_t speed;              // Zone speed (1-100)
    uint8_t blendMode;          // BlendMode enum (0-7)

    // Zone segment definitions (LED ranges on strip 1)
    // Strip 2 mirrors these with +160 offset
    uint16_t s1LeftStart;       // Left segment start (toward LED 0)
    uint16_t s1LeftEnd;         // Left segment end (inclusive)
    uint16_t s1RightStart;      // Right segment start (toward LED 159)
    uint16_t s1RightEnd;        // Right segment end (inclusive)
};

// ==================== Zone Preset ====================

/**
 * @brief Complete zone preset for NVS storage
 *
 * Stores a named configuration snapshot with all zone settings
 * and segment definitions. Fixed-size for predictable NVS blob storage.
 */
struct ZonePreset {
    uint8_t version;                            // Format version (current: 1)
    uint8_t zoneCount;                          // Number of active zones (1-4)
    char name[ZONE_PRESET_NAME_LENGTH];         // User-friendly name (null-terminated)
    ZonePresetEntry zones[ZONE_PRESET_MAX_ZONES]; // Fixed array for all zones
    uint32_t timestamp;                         // Creation timestamp (Unix epoch seconds)
    uint32_t crc32;                             // CRC32 checksum (excludes this field)

    /**
     * @brief Calculate and store CRC32 checksum
     *
     * Calculates checksum over all fields except the crc32 field itself.
     */
    void calculateChecksum();

    /**
     * @brief Validate stored checksum
     * @return true if checksum matches calculated value
     */
    bool isValid() const;
};

// ==================== Zone Preset Metadata ====================

/**
 * @brief Lightweight preset metadata for UI listing
 *
 * Used by list() to return summary information without loading
 * the full preset data.
 */
struct ZonePresetMetadata {
    uint8_t slot;                               // Slot number (0-15)
    char name[ZONE_PRESET_NAME_LENGTH];         // Preset name
    uint8_t zoneCount;                          // Number of zones
    uint32_t timestamp;                         // Creation timestamp
    bool occupied;                              // Slot contains valid preset
};

// ==================== ZonePresetManager Class ====================

/**
 * @brief Singleton manager for user-defined zone presets
 *
 * Provides CRUD operations for zone presets stored in NVS flash.
 * Integrates with ZoneComposer to save current state or apply
 * loaded presets.
 *
 * Usage:
 *   ZonePresetManager& mgr = ZonePresetManager::instance();
 *   mgr.init();
 *
 *   // Save current zone configuration
 *   mgr.saveCurrentZones(0, "My Setup", &zoneComposer);
 *
 *   // Load a saved preset
 *   mgr.applyToZones(0, &zoneComposer);
 *
 *   // List all presets
 *   ZonePresetMetadata metadata[16];
 *   uint8_t count = 16;
 *   mgr.list(metadata, count);
 */
class ZonePresetManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static ZonePresetManager& instance();

    // Prevent copying
    ZonePresetManager(const ZonePresetManager&) = delete;
    ZonePresetManager& operator=(const ZonePresetManager&) = delete;

    // ==================== Initialisation ====================

    /**
     * @brief Initialise the preset manager
     *
     * Ensures NVS is ready and scans for existing presets.
     *
     * @return true if initialised successfully
     */
    bool init();

    /**
     * @brief Check if manager is initialised
     */
    bool isInitialized() const { return m_initialized; }

    // ==================== CRUD Operations ====================

    /**
     * @brief Save a preset to a specific slot
     *
     * @param slot Slot number (0-15)
     * @param preset Preset data to save
     * @return NVSResult indicating success or failure
     */
    NVSResult save(uint8_t slot, const ZonePreset& preset);

    /**
     * @brief Load a preset from a specific slot
     *
     * @param slot Slot number (0-15)
     * @param preset Output: loaded preset data
     * @return NVSResult indicating success or failure
     */
    NVSResult load(uint8_t slot, ZonePreset& preset);

    /**
     * @brief List all preset metadata
     *
     * Scans all slots and returns metadata for occupied slots.
     *
     * @param metadata Output array (must have space for MAX_PRESETS entries)
     * @param count In: array size, Out: number of entries written
     * @return NVSResult::OK on success
     */
    NVSResult list(ZonePresetMetadata* metadata, uint8_t& count);

    /**
     * @brief Remove a preset from a slot
     *
     * @param slot Slot number (0-15)
     * @return NVSResult indicating success or failure
     */
    NVSResult remove(uint8_t slot);

    /**
     * @brief Check if a slot contains a valid preset
     *
     * @param slot Slot number (0-15)
     * @return true if slot is occupied with valid preset
     */
    bool isSlotOccupied(uint8_t slot);

    // ==================== ZoneComposer Integration ====================

    /**
     * @brief Save current ZoneComposer state as a preset
     *
     * Captures the current zone configuration from the composer
     * and saves it to the specified slot.
     *
     * @param slot Slot number (0-15)
     * @param name User-friendly name for the preset (max 31 chars)
     * @param composer Pointer to ZoneComposer instance
     * @return NVSResult indicating success or failure
     */
    NVSResult saveCurrentZones(uint8_t slot, const char* name,
                               zones::ZoneComposer* composer);

    /**
     * @brief Apply a saved preset to ZoneComposer
     *
     * Loads the preset from the specified slot and applies all
     * settings to the ZoneComposer.
     *
     * @param slot Slot number (0-15)
     * @param composer Pointer to ZoneComposer instance
     * @return NVSResult indicating success or failure
     */
    NVSResult applyToZones(uint8_t slot, zones::ZoneComposer* composer);

    // ==================== Utilities ====================

    /**
     * @brief Get the maximum number of preset slots
     */
    static constexpr uint8_t getMaxPresets() { return ZONE_PRESET_MAX_SLOTS; }

    /**
     * @brief Get the last error result
     */
    NVSResult getLastError() const { return m_lastError; }

    // ==================== Constants ====================

    static constexpr uint8_t MAX_PRESETS = ZONE_PRESET_MAX_SLOTS;
    static constexpr const char* NVS_NAMESPACE = "zones";

private:
    // Private constructor for singleton
    ZonePresetManager();
    ~ZonePresetManager() = default;

    /**
     * @brief Generate NVS key for a slot
     *
     * @param slot Slot number
     * @param buffer Output buffer (must be at least 12 chars)
     */
    void slotToKey(uint8_t slot, char* buffer) const;

    /**
     * @brief Validate preset data ranges
     *
     * @param preset Preset to validate
     * @return true if all values are within valid ranges
     */
    bool validatePreset(const ZonePreset& preset) const;

    /**
     * @brief Populate preset from ZoneComposer state
     *
     * @param preset Output preset structure
     * @param name Preset name
     * @param composer ZoneComposer to read state from
     */
    void populateFromComposer(ZonePreset& preset, const char* name,
                              zones::ZoneComposer* composer);

    /**
     * @brief Apply preset settings to ZoneComposer
     *
     * @param preset Preset to apply
     * @param composer ZoneComposer to configure
     */
    void applyToComposer(const ZonePreset& preset,
                         zones::ZoneComposer* composer);

    // Member variables
    bool m_initialized;
    NVSResult m_lastError;

    // Cached slot occupancy (refreshed on init/save/remove)
    bool m_slotOccupied[ZONE_PRESET_MAX_SLOTS];

    // Validation limits - reference centralised limits
    static constexpr uint8_t MAX_EFFECT_ID = limits::MAX_EFFECTS - 1;
    static constexpr uint8_t MIN_SPEED = 1;
    static constexpr uint8_t MAX_SPEED = 100;
    static constexpr uint8_t MAX_PALETTE_ID = limits::MAX_PALETTES - 1;
    static constexpr uint8_t MAX_BLEND_MODE = 7;

    // Preset format version
    static constexpr uint8_t PRESET_VERSION = 1;
};

// ==================== Global Access ====================

/**
 * @brief Quick access to ZonePresetManager singleton
 */
#define ZONE_PRESET_MANAGER (::lightwaveos::persistence::ZonePresetManager::instance())

} // namespace persistence
} // namespace lightwaveos
