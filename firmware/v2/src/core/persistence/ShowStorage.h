/**
 * @file ShowStorage.h
 * @brief NVS-based storage for custom shows
 *
 * LightwaveOS v2 - Show System Persistence
 *
 * Manages storage of custom shows in NVS flash memory.
 * Supports up to MAX_CUSTOM_SHOWS custom shows.
 *
 * Storage Format:
 * - Metadata index: "shows" namespace, "index" key (uint8_t count)
 * - Individual shows: "shows" namespace, "show_N" keys (N = 0-9)
 * - Each show stored as binary blob with header + scene data
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../shows/ShowTranslator.h"
#include "../shows/ShowTypes.h"
#include "NVSManager.h"
#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace persistence {

// ============================================================================
// Storage Constants
// ============================================================================

static constexpr const char* SHOW_STORAGE_NS = "shows";
static constexpr const char* SHOW_INDEX_KEY = "index";
static constexpr uint8_t MAX_CUSTOM_SHOWS = 10;
static constexpr uint8_t MAX_SCENES_PER_SHOW = 50;

// Public constants for external use
namespace ShowStorageConstants {
    constexpr uint8_t MAX_CUSTOM_SHOWS = 10;
    constexpr uint8_t MAX_SCENES_PER_SHOW = 50;
}

// ============================================================================
// Stored Show Header
// ============================================================================

/**
 * @brief Header for stored show (fixed-size, followed by variable scene data)
 */
struct StoredShowHeader {
    uint8_t version;                // Format version (currently 1)
    char id[32];                    // Show ID (e.g., "show-abc123")
    char name[32];                  // Show name
    uint32_t durationMs;            // Total duration
    uint8_t sceneCount;             // Number of scenes
    uint8_t _reserved[3];          // Padding for alignment

    StoredShowHeader()
        : version(1)
        , durationMs(0)
        , sceneCount(0)
    {
        id[0] = '\0';
        name[0] = '\0';
        memset(_reserved, 0, sizeof(_reserved));
    }
};

// ============================================================================
// Show Storage
// ============================================================================

/**
 * @brief Manages custom show storage in NVS
 */
class ShowStorage {
public:
    /**
     * @brief Get singleton instance
     */
    static ShowStorage& instance();

    // Prevent copying
    ShowStorage(const ShowStorage&) = delete;
    ShowStorage& operator=(const ShowStorage&) = delete;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * @brief Initialize storage (ensure NVS is ready)
     * @return true if initialized successfully
     */
    bool init();

    // ========================================================================
    // Show Management
    // ========================================================================

    /**
     * @brief Save a custom show to NVS
     * @param id Show ID (unique identifier)
     * @param name Show name
     * @param durationMs Total duration in milliseconds
     * @param scenes Array of scenes
     * @param sceneCount Number of scenes
     * @return true if saved successfully
     */
    bool saveShow(
        const char* id,
        const char* name,
        uint32_t durationMs,
        const shows::TimelineScene* scenes,
        uint8_t sceneCount
    );

    /**
     * @brief Load a custom show from NVS
     * @param id Show ID
     * @param outName Output buffer for name (must be at least 32 bytes)
     * @param outDurationMs Output: duration in milliseconds
     * @param outScenes Output array for scenes (must be pre-allocated)
     * @param outSceneCount Output: number of scenes loaded
     * @param maxScenes Maximum scenes that can be loaded
     * @return true if loaded successfully
     */
    bool loadShow(
        const char* id,
        char* outName,
        size_t nameLen,
        uint32_t& outDurationMs,
        shows::TimelineScene* outScenes,
        uint8_t& outSceneCount,
        uint8_t maxScenes
    );

    /**
     * @brief Delete a custom show from NVS
     * @param id Show ID
     * @return true if deleted successfully
     */
    bool deleteShow(const char* id);

    /**
     * @brief List all custom shows
     * @param outShows Output array for show info (must be pre-allocated)
     * @param outCount Output: number of shows found
     * @param maxShows Maximum shows to return
     * @return true if listing successful
     */
    bool listShows(ShowInfo* outShows, uint8_t& outCount, uint8_t maxShows);

    /**
     * @brief Get number of custom shows stored
     * @return Number of custom shows (0-10)
     */
    uint8_t getCustomShowCount();

    /**
     * @brief Check if storage has space for another show
     * @return true if space available
     */
    bool hasSpace();

    /**
     * @brief Get last error code
     * @return Last NVSResult
     */
    NVSResult getLastError() const { return m_lastError; }

private:
    ShowStorage() : m_lastError(NVSResult::OK) {}
    ~ShowStorage() = default;

    /**
     * @brief Find slot index for show ID
     * @param id Show ID
     * @return Slot index (0-9) or 0xFF if not found
     */
    uint8_t findShowSlot(const char* id);

    /**
     * @brief Find next available slot
     * @return Slot index (0-9) or 0xFF if full
     */
    uint8_t findFreeSlot();

    /**
     * @brief Generate storage key for slot
     * @param slot Slot index (0-9)
     * @param outKey Output buffer (must be at least 16 bytes)
     */
    void getStorageKey(uint8_t slot, char* outKey);

    /**
     * @brief Update show index count
     * @return true if updated successfully
     */
    bool updateIndex();

    NVSResult m_lastError;
};

// Convenience macro
#define SHOW_STORAGE (ShowStorage::instance())

} // namespace persistence
} // namespace lightwaveos

