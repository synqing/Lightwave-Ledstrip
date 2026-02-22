/**
 * @file DynamicShowStore.h
 * @brief PSRAM-backed runtime storage for uploaded ShowBundle shows
 *
 * Manages up to MAX_DYNAMIC_SHOWS uploaded shows in PSRAM.
 * Built-in PROGMEM shows remain in BuiltinShows.h; this module
 * handles dynamically uploaded shows from PRISM Studio.
 *
 * Memory is allocated from PSRAM via heap_caps_malloc() at upload time
 * and freed on delete or overwrite. No heap allocation in tick paths.
 *
 * Thread safety: All mutations happen on Core 0 (network handlers).
 * ShowDirectorActor reads via const pointers, so no mutex is needed
 * provided registration/unregistration is atomic (pointer swap).
 */

#pragma once

#include <Arduino.h>
#include <cstring>

#include "ShowTypes.h"

#ifndef NATIVE_BUILD
#include <esp_log.h>
#include <esp_heap_caps.h>
#endif

namespace prism {

/// Maximum number of dynamically uploaded shows (PSRAM budget: ~10KB)
static constexpr uint8_t MAX_DYNAMIC_SHOWS = 4;

/// Maximum cues per uploaded show
static constexpr uint16_t MAX_CUES_PER_SHOW = 512;

/// Maximum chapters per uploaded show
static constexpr uint8_t MAX_CHAPTERS_PER_SHOW = 32;

/// Maximum JSON payload size for a ShowBundle upload (bytes)
static constexpr size_t MAX_SHOW_JSON_SIZE = 32768;

/// Maximum string ID length (including null terminator)
static constexpr uint8_t MAX_SHOW_ID_LEN = 33;

/// Maximum display name length (including null terminator)
static constexpr uint8_t MAX_SHOW_NAME_LEN = 65;

/// Maximum chapter name length (including null terminator)
static constexpr uint8_t MAX_CHAPTER_NAME_LEN = 33;


/**
 * @brief Runtime show data allocated in PSRAM
 *
 * Unlike PROGMEM ShowDefinition which uses const char* pointers into flash,
 * DynamicShowData owns its string storage inline.
 */
struct DynamicShowData {
    // Owned string storage
    char id[MAX_SHOW_ID_LEN];
    char name[MAX_SHOW_NAME_LEN];
    char chapterNames[MAX_CHAPTERS_PER_SHOW][MAX_CHAPTER_NAME_LEN];

    // Show metadata
    uint32_t totalDurationMs;
    float bpm;
    bool looping;

    // Chapter array (owned, PSRAM)
    ShowChapter* chapters;
    uint8_t chapterCount;

    // Cue array (owned, PSRAM)
    ShowCue* cues;
    uint16_t cueCount;

    // ShowDefinition facade for ShowDirectorActor compatibility
    // The director reads via const ShowDefinition*, so we provide one
    // whose pointers reference our owned arrays.
    ShowDefinition definition;

    // RAM usage tracking
    size_t totalRamBytes;

    /// Build the ShowDefinition facade from owned data
    void buildDefinition() {
        definition.id = id;
        definition.name = name;
        definition.totalDurationMs = totalDurationMs;
        definition.chapterCount = chapterCount;
        definition.totalCues = static_cast<uint8_t>(cueCount > 255 ? 255 : cueCount);
        definition.looping = looping;
        definition.chapters = chapters;
        definition.cues = cues;

        // Wire chapter name pointers to our owned storage
        for (uint8_t i = 0; i < chapterCount; i++) {
            chapters[i].name = chapterNames[i];
        }
    }
};


/**
 * @brief Store for dynamically uploaded shows
 *
 * Slot-based storage with atomic pointer swap for thread-safe
 * registration with ShowDirectorActor.
 */
class DynamicShowStore {
public:
    DynamicShowStore() {
        memset(m_slots, 0, sizeof(m_slots));
        memset(m_occupied, 0, sizeof(m_occupied));
    }

    ~DynamicShowStore() {
        for (uint8_t i = 0; i < MAX_DYNAMIC_SHOWS; i++) {
            freeSlot(i);
        }
    }

    /**
     * @brief Find a show by string ID
     * @param id Show ID string
     * @return Slot index, or -1 if not found
     */
    int8_t findById(const char* id) const {
        for (uint8_t i = 0; i < MAX_DYNAMIC_SHOWS; i++) {
            if (m_occupied[i] && strcmp(m_slots[i]->id, id) == 0) {
                return static_cast<int8_t>(i);
            }
        }
        return -1;
    }

    /**
     * @brief Find a free slot
     * @return Slot index, or -1 if store is full
     */
    int8_t findFreeSlot() const {
        for (uint8_t i = 0; i < MAX_DYNAMIC_SHOWS; i++) {
            if (!m_occupied[i]) {
                return static_cast<int8_t>(i);
            }
        }
        return -1;
    }

    /**
     * @brief Allocate a DynamicShowData in PSRAM for the given cue/chapter counts
     * @param cueCount Number of cues
     * @param chapterCount Number of chapters
     * @return Pointer to allocated data, or nullptr if allocation fails
     */
    DynamicShowData* allocateShowData(uint16_t cueCount, uint8_t chapterCount) {
        if (cueCount > MAX_CUES_PER_SHOW || chapterCount > MAX_CHAPTERS_PER_SHOW) {
            return nullptr;
        }

        size_t totalSize = sizeof(DynamicShowData)
            + sizeof(ShowCue) * cueCount
            + sizeof(ShowChapter) * chapterCount;

#ifndef NATIVE_BUILD
        // Allocate from PSRAM
        uint8_t* block = static_cast<uint8_t*>(
            heap_caps_malloc(totalSize, MALLOC_CAP_SPIRAM)
        );
#else
        uint8_t* block = static_cast<uint8_t*>(malloc(totalSize));
#endif

        if (!block) {
            return nullptr;
        }

        memset(block, 0, totalSize);

        // Layout: [DynamicShowData] [ShowCue * cueCount] [ShowChapter * chapterCount]
        DynamicShowData* data = reinterpret_cast<DynamicShowData*>(block);
        data->cues = reinterpret_cast<ShowCue*>(block + sizeof(DynamicShowData));
        data->chapters = reinterpret_cast<ShowChapter*>(
            block + sizeof(DynamicShowData) + sizeof(ShowCue) * cueCount
        );
        data->cueCount = cueCount;
        data->chapterCount = chapterCount;
        data->totalRamBytes = totalSize;

        return data;
    }

    /**
     * @brief Register a fully populated DynamicShowData into a slot
     * @param slot Slot index (0 to MAX_DYNAMIC_SHOWS-1)
     * @param data Fully populated show data (ownership transferred)
     * @return true on success
     */
    bool registerShow(uint8_t slot, DynamicShowData* data) {
        if (slot >= MAX_DYNAMIC_SHOWS || data == nullptr) {
            return false;
        }

        // Free existing show in this slot if any
        if (m_occupied[slot]) {
            freeSlot(slot);
        }

        // Build the ShowDefinition facade
        data->buildDefinition();

        // Atomic-ish pointer swap (single write on 32-bit arch)
        m_slots[slot] = data;
        m_occupied[slot] = true;

        return true;
    }

    /**
     * @brief Delete a show by slot index
     * @param slot Slot index
     */
    void deleteShow(uint8_t slot) {
        if (slot < MAX_DYNAMIC_SHOWS) {
            freeSlot(slot);
        }
    }

    /**
     * @brief Delete a show by string ID
     * @param id Show ID
     * @return true if found and deleted
     */
    bool deleteShowById(const char* id) {
        int8_t slot = findById(id);
        if (slot < 0) return false;
        freeSlot(static_cast<uint8_t>(slot));
        return true;
    }

    /**
     * @brief Get const ShowDefinition pointer for ShowDirectorActor
     * @param slot Slot index
     * @return Pointer to ShowDefinition, or nullptr
     */
    const ShowDefinition* getDefinition(uint8_t slot) const {
        if (slot >= MAX_DYNAMIC_SHOWS || !m_occupied[slot]) {
            return nullptr;
        }
        return &m_slots[slot]->definition;
    }

    /**
     * @brief Get show data for a slot (for API responses)
     * @param slot Slot index
     * @return Pointer to DynamicShowData, or nullptr
     */
    const DynamicShowData* getShowData(uint8_t slot) const {
        if (slot >= MAX_DYNAMIC_SHOWS || !m_occupied[slot]) {
            return nullptr;
        }
        return m_slots[slot];
    }

    /// Number of occupied slots
    uint8_t count() const {
        uint8_t n = 0;
        for (uint8_t i = 0; i < MAX_DYNAMIC_SHOWS; i++) {
            if (m_occupied[i]) n++;
        }
        return n;
    }

    /// Total RAM used by all stored shows
    size_t totalRamUsage() const {
        size_t total = 0;
        for (uint8_t i = 0; i < MAX_DYNAMIC_SHOWS; i++) {
            if (m_occupied[i] && m_slots[i]) {
                total += m_slots[i]->totalRamBytes;
            }
        }
        return total;
    }

    /// Check if a slot is occupied
    bool isOccupied(uint8_t slot) const {
        return slot < MAX_DYNAMIC_SHOWS && m_occupied[slot];
    }

private:
    void freeSlot(uint8_t slot) {
        if (slot >= MAX_DYNAMIC_SHOWS) return;
        m_occupied[slot] = false;
        if (m_slots[slot]) {
#ifndef NATIVE_BUILD
            heap_caps_free(m_slots[slot]);
#else
            free(m_slots[slot]);
#endif
            m_slots[slot] = nullptr;
        }
    }

    DynamicShowData* m_slots[MAX_DYNAMIC_SHOWS];
    bool m_occupied[MAX_DYNAMIC_SHOWS];
};

} // namespace prism
