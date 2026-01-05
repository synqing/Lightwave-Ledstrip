/**
 * @file ShowStorage.cpp
 * @brief Implementation of ShowStorage
 */

#include "ShowStorage.h"
#include <cstring>
#include <cstdio>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace persistence {

// ============================================================================
// Singleton
// ============================================================================

ShowStorage& ShowStorage::instance() {
    static ShowStorage s_instance;
    return s_instance;
}

// ============================================================================
// Initialization
// ============================================================================

bool ShowStorage::init() {
    if (!NVS_MANAGER.isInitialized()) {
        if (!NVS_MANAGER.init()) {
            m_lastError = NVSResult::NOT_INITIALIZED;
            return false;
        }
    }
    return true;
}

// ============================================================================
// Storage Key Generation
// ============================================================================

void ShowStorage::getStorageKey(uint8_t slot, char* outKey) {
    if (slot < MAX_CUSTOM_SHOWS) {
        snprintf(outKey, 16, "show_%u", slot);
    } else {
        outKey[0] = '\0';
    }
}

// ============================================================================
// Slot Management
// ============================================================================

uint8_t ShowStorage::findShowSlot(const char* id) {
    if (!id || id[0] == '\0') {
        return 0xFF;
    }

    // Check each slot
    for (uint8_t i = 0; i < MAX_CUSTOM_SHOWS; i++) {
        char key[16];
        getStorageKey(i, key);

        // Try to load header to check if show exists
        StoredShowHeader header;
        NVSResult result = NVS_MANAGER.loadBlob(SHOW_STORAGE_NS, key, &header, sizeof(header));
        
        if (result == NVSResult::OK) {
            // Check if ID matches
            if (strcmp(header.id, id) == 0) {
                return i;
            }
        }
    }

    return 0xFF;  // Not found
}

uint8_t ShowStorage::findFreeSlot() {
    // Check each slot for empty space
    for (uint8_t i = 0; i < MAX_CUSTOM_SHOWS; i++) {
        char key[16];
        getStorageKey(i, key);

        // Try to load header - if NOT_FOUND, slot is free
        StoredShowHeader header;
        NVSResult result = NVS_MANAGER.loadBlob(SHOW_STORAGE_NS, key, &header, sizeof(header));
        
        if (result == NVSResult::NOT_FOUND) {
            return i;  // Free slot found
        }
    }

    return 0xFF;  // No free slots
}

bool ShowStorage::updateIndex() {
    // Count existing shows
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_CUSTOM_SHOWS; i++) {
        char key[16];
        getStorageKey(i, key);

        StoredShowHeader header;
        NVSResult result = NVS_MANAGER.loadBlob(SHOW_STORAGE_NS, key, &header, sizeof(header));
        if (result == NVSResult::OK) {
            count++;
        }
    }

    // Save count
    m_lastError = NVS_MANAGER.saveUint8(SHOW_STORAGE_NS, SHOW_INDEX_KEY, count);
    return m_lastError == NVSResult::OK;
}

// ============================================================================
// Show Management
// ============================================================================

bool ShowStorage::saveShow(
    const char* id,
    const char* name,
    uint32_t durationMs,
    const shows::TimelineScene* scenes,
    uint8_t sceneCount
) {
    if (!init()) {
        return false;
    }

    if (!id || !name || !scenes || sceneCount == 0 || sceneCount > MAX_SCENES_PER_SHOW) {
        m_lastError = NVSResult::INVALID_HANDLE;
        return false;
    }

    // Find slot (existing or free)
    uint8_t slot = findShowSlot(id);
    if (slot == 0xFF) {
        slot = findFreeSlot();
        if (slot == 0xFF) {
            m_lastError = NVSResult::FLASH_ERROR;  // No space
            return false;
        }
    }

    // Prepare header
    StoredShowHeader header;
    header.version = 1;
    strncpy(header.id, id, sizeof(header.id) - 1);
    header.id[sizeof(header.id) - 1] = '\0';
    strncpy(header.name, name, sizeof(header.name) - 1);
    header.name[sizeof(header.name) - 1] = '\0';
    header.durationMs = durationMs;
    header.sceneCount = sceneCount;

    // Calculate total size
    size_t headerSize = sizeof(StoredShowHeader);
    size_t scenesSize = sizeof(shows::TimelineScene) * sceneCount;
    size_t totalSize = headerSize + scenesSize;

    // Allocate buffer for header + scenes
    uint8_t* buffer = new uint8_t[totalSize];
    if (!buffer) {
        m_lastError = NVSResult::FLASH_ERROR;
        return false;
    }

    // Copy header
    memcpy(buffer, &header, headerSize);
    // Copy scenes
    memcpy(buffer + headerSize, scenes, scenesSize);

    // Save to NVS
    char key[16];
    getStorageKey(slot, key);
    m_lastError = NVS_MANAGER.saveBlob(SHOW_STORAGE_NS, key, buffer, totalSize);

    delete[] buffer;

    if (m_lastError != NVSResult::OK) {
        return false;
    }

    // Update index
    return updateIndex();
}

bool ShowStorage::loadShow(
    const char* id,
    char* outName,
    size_t nameLen,
    uint32_t& outDurationMs,
    shows::TimelineScene* outScenes,
    uint8_t& outSceneCount,
    uint8_t maxScenes
) {
    if (!init()) {
        return false;
    }

    if (!id || !outName || nameLen == 0 || !outScenes || maxScenes == 0) {
        m_lastError = NVSResult::INVALID_HANDLE;
        return false;
    }

    // Find slot
    uint8_t slot = findShowSlot(id);
    if (slot == 0xFF) {
        m_lastError = NVSResult::NOT_FOUND;
        return false;
    }

    // Load header first to get size
    char key[16];
    getStorageKey(slot, key);

    StoredShowHeader header;
    m_lastError = NVS_MANAGER.loadBlob(SHOW_STORAGE_NS, key, &header, sizeof(header));
    if (m_lastError != NVSResult::OK) {
        return false;
    }

    // Validate
    if (header.sceneCount == 0 || header.sceneCount > MAX_SCENES_PER_SHOW) {
        m_lastError = NVSResult::SIZE_MISMATCH;
        return false;
    }

    if (header.sceneCount > maxScenes) {
        m_lastError = NVSResult::SIZE_MISMATCH;
        return false;
    }

    // Calculate total size
    size_t headerSize = sizeof(StoredShowHeader);
    size_t scenesSize = sizeof(shows::TimelineScene) * header.sceneCount;
    size_t totalSize = headerSize + scenesSize;

    // Allocate buffer
    uint8_t* buffer = new uint8_t[totalSize];
    if (!buffer) {
        m_lastError = NVSResult::FLASH_ERROR;
        return false;
    }

    // Load full data
    m_lastError = NVS_MANAGER.loadBlob(SHOW_STORAGE_NS, key, buffer, totalSize);
    if (m_lastError != NVSResult::OK) {
        delete[] buffer;
        return false;
    }

    // Extract header
    memcpy(&header, buffer, headerSize);
    
    // Extract scenes
    memcpy(outScenes, buffer + headerSize, scenesSize);

    // Copy name
    strncpy(outName, header.name, nameLen - 1);
    outName[nameLen - 1] = '\0';
    outDurationMs = header.durationMs;
    outSceneCount = header.sceneCount;

    delete[] buffer;
    return true;
}

bool ShowStorage::deleteShow(const char* id) {
    if (!init()) {
        return false;
    }

    if (!id) {
        m_lastError = NVSResult::INVALID_HANDLE;
        return false;
    }

    // Find slot
    uint8_t slot = findShowSlot(id);
    if (slot == 0xFF) {
        m_lastError = NVSResult::NOT_FOUND;
        return false;
    }

    // Erase key
    char key[16];
    getStorageKey(slot, key);
    m_lastError = NVS_MANAGER.eraseKey(SHOW_STORAGE_NS, key);

    if (m_lastError != NVSResult::OK) {
        return false;
    }

    // Update index
    return updateIndex();
}

bool ShowStorage::listShows(ShowInfo* outShows, uint8_t& outCount, uint8_t maxShows) {
    if (!init()) {
        return false;
    }

    if (!outShows || maxShows == 0) {
        m_lastError = NVSResult::INVALID_HANDLE;
        return false;
    }

    outCount = 0;

    // Iterate through all slots
    for (uint8_t i = 0; i < MAX_CUSTOM_SHOWS && outCount < maxShows; i++) {
        char key[16];
        getStorageKey(i, key);

        StoredShowHeader header;
        NVSResult result = NVS_MANAGER.loadBlob(SHOW_STORAGE_NS, key, &header, sizeof(header));
        
        if (result == NVSResult::OK) {
            // Create ShowInfo entry
            ShowInfo& info = outShows[outCount++];
            // Note: ShowInfo uses uint8_t id, but we have string IDs
            // We'll use slot index as ID for now (custom shows start at 100)
            info.id = 100 + i;  // Custom shows: 100-109
            // Name: ShowInfo.name is const char*, but we need to store it
            // Since ShowInfo is used for built-in shows (PROGMEM), we'll need
            // a different approach. For now, return slot-based ID and caller
            // should load full show to get name. This is a limitation of the
            // current ShowInfo structure.
            info.name = nullptr;  // Caller must load show to get name
            info.durationMs = header.durationMs;
            info.looping = false;  // Custom shows don't loop by default
        }
    }

    m_lastError = NVSResult::OK;
    return true;
}

uint8_t ShowStorage::getCustomShowCount() {
    if (!init()) {
        return 0;
    }

    return NVS_MANAGER.loadUint8(SHOW_STORAGE_NS, SHOW_INDEX_KEY, 0);
}

bool ShowStorage::hasSpace() {
    return getCustomShowCount() < MAX_CUSTOM_SHOWS;
}

} // namespace persistence
} // namespace lightwaveos
