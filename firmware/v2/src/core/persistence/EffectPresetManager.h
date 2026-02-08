/**
 * @file EffectPresetManager.h
 * @brief Effect preset persistence manager for NVS storage
 *
 * LightwaveOS v2 - Persistence System
 *
 * Manages saving and loading of single-effect presets to NVS flash.
 * Stores effect configuration including visual parameters and semantic
 * expression controls for quick recall of favourite effect setups.
 *
 * Features:
 * - Up to 16 named effect presets
 * - Stores effect ID, palette, brightness, speed, and expression parameters
 * - CRC32 checksum validation for data integrity
 * - Thread-safe operations via NVSManager
 * - Integration with RendererActor for current state capture
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "NVSManager.h"

// Forward declarations
namespace lightwaveos { namespace actors { class RendererActor; } }

namespace lightwaveos {
namespace persistence {

// ==================== Effect Preset Structure ====================

/**
 * @brief Serialisable effect preset for NVS storage
 *
 * Contains all parameters needed to fully restore an effect configuration
 * including visual controls (brightness, speed) and semantic expression
 * parameters (mood, trails, intensity, etc.).
 */
struct EffectPreset {
    static constexpr uint8_t CURRENT_VERSION = 1;
    static constexpr size_t NAME_MAX_LEN = 32;

    uint8_t version = CURRENT_VERSION;  // Format version for future compatibility

    // Core effect settings
    uint8_t effectId = 0;               // Effect ID (0-255)
    uint8_t paletteId = 0;              // Palette ID (0-74)
    uint8_t brightness = 96;            // Brightness (0-255)
    uint8_t speed = 10;                 // Animation speed (1-100)

    // User-friendly name
    char name[NAME_MAX_LEN] = {0};      // Null-terminated preset name

    // Expression/semantic parameters
    uint8_t mood = 128;                 // Mood parameter (0-255)
    uint8_t trails = 128;               // Trail/fade amount (0-255)
    uint8_t hue = 0;                    // Global hue offset (0-255)
    uint8_t saturation = 255;           // Colour saturation (0-255)
    uint8_t intensity = 128;            // Effect intensity/amplitude (0-255)
    uint8_t complexity = 128;           // Effect complexity/detail (0-255)
    uint8_t variation = 0;              // Effect variation/mode (0-255)

    // Metadata
    uint32_t timestamp = 0;             // Creation timestamp (Unix epoch seconds)

    // Data integrity
    uint32_t crc32 = 0;                 // CRC32 checksum (excludes this field)

    /**
     * @brief Calculate and store CRC32 checksum
     *
     * Must be called before saving to NVS. The checksum covers
     * all fields except the crc32 field itself.
     */
    void calculateChecksum();

    /**
     * @brief Validate stored checksum against calculated value
     * @return true if checksum matches and version is compatible
     */
    bool isValid() const;

    /**
     * @brief Reset preset to default values
     */
    void reset();
};

// ==================== Preset Metadata (for listing) ====================

/**
 * @brief Lightweight metadata for preset listing without full data load
 */
struct EffectPresetMetadata {
    uint8_t slot = 0;                   // Slot index (0-15)
    char name[EffectPreset::NAME_MAX_LEN] = {0};
    uint8_t effectId = 0;
    uint8_t paletteId = 0;
    uint32_t timestamp = 0;
    bool occupied = false;              // True if slot contains valid preset
};

// ==================== EffectPresetManager Class ====================

/**
 * @brief Singleton manager for effect preset persistence
 *
 * Usage:
 *   EffectPresetManager& mgr = EffectPresetManager::instance();
 *   mgr.init();
 *
 *   // Save current effect as preset
 *   mgr.saveCurrentEffect(0, "My Favourite", renderer);
 *
 *   // Load a preset
 *   EffectPreset preset;
 *   if (mgr.load(0, preset) == NVSResult::OK) {
 *       // Apply preset to renderer...
 *   }
 *
 *   // List all presets
 *   EffectPresetMetadata meta[16];
 *   uint8_t count = 0;
 *   mgr.list(meta, count);
 */
class EffectPresetManager {
public:
    static constexpr uint8_t MAX_PRESETS = 16;
    static constexpr const char* NVS_NAMESPACE = "effects";

    /**
     * @brief Get the singleton instance
     */
    static EffectPresetManager& instance();

    // Prevent copying
    EffectPresetManager(const EffectPresetManager&) = delete;
    EffectPresetManager& operator=(const EffectPresetManager&) = delete;

    // ==================== Initialisation ====================

    /**
     * @brief Initialise the preset manager
     *
     * Should be called after NVSManager::init(). Scans existing
     * presets and builds the slot occupancy map.
     *
     * @return true if initialisation successful
     */
    bool init();

    /**
     * @brief Check if manager is initialised
     */
    bool isInitialised() const { return m_initialised; }

    // ==================== CRUD Operations ====================

    /**
     * @brief Save a preset to the specified slot
     *
     * The preset's checksum is automatically calculated before saving.
     * Overwrites any existing preset in the slot.
     *
     * @param slot Slot index (0-15)
     * @param preset Preset data to save
     * @return NVSResult indicating success or failure
     */
    NVSResult save(uint8_t slot, const EffectPreset& preset);

    /**
     * @brief Load a preset from the specified slot
     *
     * Validates the checksum after loading.
     *
     * @param slot Slot index (0-15)
     * @param preset Output: loaded preset data
     * @return NVSResult::OK on success, NOT_FOUND if slot empty,
     *         CHECKSUM_ERROR if data corrupted
     */
    NVSResult load(uint8_t slot, EffectPreset& preset);

    /**
     * @brief List all saved presets with metadata
     *
     * Populates the metadata array with information about each
     * occupied slot. Unoccupied slots have occupied=false.
     *
     * @param metadata Output array (must have MAX_PRESETS elements)
     * @param count Output: number of occupied slots found
     * @return NVSResult::OK on success
     */
    NVSResult list(EffectPresetMetadata* metadata, uint8_t& count);

    /**
     * @brief Remove a preset from the specified slot
     *
     * @param slot Slot index (0-15)
     * @return NVSResult::OK on success, NOT_FOUND if slot already empty
     */
    NVSResult remove(uint8_t slot);

    // ==================== Convenience Methods ====================

    /**
     * @brief Save the current effect state from RendererActor as a preset
     *
     * Captures: effectId, paletteId, brightness, speed, hue, intensity,
     * saturation, complexity, variation, mood, and fadeAmount (as trails).
     *
     * @param slot Slot index (0-15)
     * @param name User-friendly preset name (max 31 chars)
     * @param renderer Pointer to RendererActor for state capture
     * @return NVSResult indicating success or failure
     */
    NVSResult saveCurrentEffect(uint8_t slot, const char* name,
                                actors::RendererActor* renderer);

    /**
     * @brief Check if a slot contains a valid preset
     *
     * @param slot Slot index (0-15)
     * @return true if slot is occupied with a valid preset
     */
    bool isSlotOccupied(uint8_t slot) const;

    /**
     * @brief Get the number of saved presets
     *
     * @return Count of occupied preset slots (0-16)
     */
    uint8_t getPresetCount() const;

    /**
     * @brief Find the first available (empty) slot
     *
     * @return Slot index (0-15) or -1 if all slots occupied
     */
    int8_t findFreeSlot() const;

    // ==================== Utility ====================

    /**
     * @brief Get the last error from operations
     */
    NVSResult getLastError() const { return m_lastError; }

    /**
     * @brief Convert slot index to NVS key name
     *
     * @param slot Slot index (0-15)
     * @param key Output buffer (must be at least 16 bytes)
     */
    static void makeKey(uint8_t slot, char* key);

private:
    EffectPresetManager();
    ~EffectPresetManager() = default;

    bool m_initialised;
    NVSResult m_lastError;

    // Slot occupancy bitmap (bit N = slot N occupied)
    uint16_t m_slotBitmap;

    /**
     * @brief Scan NVS and populate slot occupancy bitmap
     */
    void scanSlots();

    /**
     * @brief Update slot bitmap after save/remove
     */
    void updateSlotBitmap(uint8_t slot, bool occupied);
};

// ==================== Global Access Macro ====================

/**
 * @brief Quick access to EffectPresetManager singleton
 */
#define EFFECT_PRESET_MANAGER (::lightwaveos::persistence::EffectPresetManager::instance())

} // namespace persistence
} // namespace lightwaveos
