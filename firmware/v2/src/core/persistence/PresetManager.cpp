/**
 * @file PresetManager.cpp
 * @brief File-based preset library manager implementation
 *
 * LightwaveOS v2 - Preset Library System
 */

#include "PresetManager.h"
#include <Arduino.h>
#include <time.h>
#include <cstring>
#define LW_LOG_TAG "PresetMgr"
#include "../../utils/Log.h"

namespace lightwaveos {
namespace persistence {

// ==================== Constructor ====================

PresetManager::PresetManager()
    : m_initialized(false)
{
}

// ==================== Initialization ====================

bool PresetManager::init() {
    if (m_initialized) {
        return true;
    }

    // Verify LittleFS is accessible (don't try to create directory - it will be created on first file write)
    if (!ensurePresetsDir()) {
        // LittleFS not mounted yet - this is expected if WebServer hasn't started
        // Return false gracefully without logging error (auto-init will retry later)
        return false;
    }

    m_initialized = true;
    LW_LOGI("PresetManager initialized");
    return true;
}

bool PresetManager::ensurePresetsDir() {
    // LittleFS should already be mounted by WebServer::begin()
    // Just verify we can access the filesystem - don't try to create directory
    // LittleFS will create the directory path automatically when first file is written
    
    // Verify LittleFS is accessible by checking if we can open root
    File root = LittleFS.open("/", "r");
    if (!root || !root.isDirectory()) {
        // Don't log error - LittleFS may not be mounted yet (WebServer hasn't started)
        // This is expected during early initialization
        if (root) root.close();
        return false;
    }
    root.close();

    // Directory will be created automatically when first preset is saved
    // No need to pre-create it
    return true;
}

bool PresetManager::isLittleFSMounted() {
    // Try to open root directory - this will fail if filesystem isn't mounted
    File root = LittleFS.open("/", "r");
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return false;
    }
    root.close();
    return true;
}

// ==================== Preset Operations ====================

bool PresetManager::savePreset(const char* name, const ZoneConfigData& config,
                                const char* description, const char* author) {
    // Check if filesystem is mounted before attempting any operations
    if (!isLittleFSMounted()) {
        LW_LOGW("Cannot save preset: LittleFS is not mounted");
        return false;
    }
    
    if (!m_initialized && !init()) {
        LW_LOGW("Cannot save preset: PresetManager initialization failed");
        return false;
    }

    String sanitizedName = sanitizeName(name);
    if (sanitizedName.length() == 0) {
        LW_LOGE("Invalid preset name");
        return false;
    }

    String filepath = getPresetPath(sanitizedName.c_str());

    // Create JSON document
    StaticJsonDocument<2048> doc;

    // Add metadata
    doc["name"] = sanitizedName;
    if (description) {
        doc["description"] = description;
    }
    if (author) {
        doc["author"] = author;
    }
    
    // Add timestamp
    time_t now = time(nullptr);
    char timeStr[32];
    if (now > 0) {
        struct tm* timeinfo = gmtime(&now);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    } else {
        strcpy(timeStr, "1970-01-01T00:00:00Z");
    }
    doc["created"] = timeStr;
    doc["version"] = PRESET_FORMAT_VERSION;

    // Export zone configuration
    if (!exportToJson(config, doc, nullptr)) {
        LW_LOGE("Failed to export config to JSON");
        return false;
    }

    // Ensure presets directory exists
    if (!LittleFS.exists(PRESETS_DIR)) {
        if (!LittleFS.mkdir(PRESETS_DIR)) {
            LW_LOGE("Failed to create presets directory: %s", PRESETS_DIR);
            return false;
        }
        LW_LOGI("Created presets directory: %s", PRESETS_DIR);
    }

    // Write to file
    File file = LittleFS.open(filepath, "w");
    if (!file) {
        LW_LOGE("Failed to open preset file for writing: %s", filepath.c_str());
        LW_LOGE("LittleFS may not be mounted or directory doesn't exist");
        return false;
    }

    serializeJson(doc, file);
    file.close();

    LW_LOGI("Preset saved: %s", sanitizedName.c_str());
    return true;
}

bool PresetManager::loadPreset(const char* name, ZoneConfigData& config) {
    if (!m_initialized && !init()) {
        return false;
    }

    String sanitizedName = sanitizeName(name);
    String filepath = getPresetPath(sanitizedName.c_str());

    if (!LittleFS.exists(filepath)) {
        LW_LOGE("Preset not found: %s", sanitizedName.c_str());
        return false;
    }

    File file = LittleFS.open(filepath, "r");
    if (!file) {
        LW_LOGE("Failed to open preset file: %s", filepath.c_str());
        return false;
    }

    // Parse JSON
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LW_LOGE("Failed to parse preset JSON: %s", error.c_str());
        return false;
    }

    // Import zone configuration
    if (!importFromJson(doc, config, nullptr)) {
        LW_LOGE("Failed to import config from JSON");
        return false;
    }

    LW_LOGI("Preset loaded: %s", sanitizedName.c_str());
    return true;
}

bool PresetManager::deletePreset(const char* name) {
    if (!m_initialized && !init()) {
        return false;
    }

    String sanitizedName = sanitizeName(name);
    String filepath = getPresetPath(sanitizedName.c_str());

    if (!LittleFS.exists(filepath)) {
        LW_LOGE("Preset not found: %s", sanitizedName.c_str());
        return false;
    }

    if (!LittleFS.remove(filepath)) {
        LW_LOGE("Failed to delete preset file: %s", filepath.c_str());
        return false;
    }

    LW_LOGI("Preset deleted: %s", sanitizedName.c_str());
    return true;
}

bool PresetManager::renamePreset(const char* oldName, const char* newName) {
    if (!m_initialized && !init()) {
        return false;
    }

    String sanitizedOldName = sanitizeName(oldName);
    String sanitizedNewName = sanitizeName(newName);
    
    if (sanitizedOldName.length() == 0 || sanitizedNewName.length() == 0) {
        LW_LOGE("Invalid preset name for rename");
        return false;
    }

    String oldPath = getPresetPath(sanitizedOldName.c_str());
    String newPath = getPresetPath(sanitizedNewName.c_str());

    if (!LittleFS.exists(oldPath)) {
        LW_LOGE("Preset not found: %s", sanitizedOldName.c_str());
        return false;
    }

    if (LittleFS.exists(newPath)) {
        LW_LOGE("Target preset already exists: %s", sanitizedNewName.c_str());
        return false;
    }

    // Try LittleFS rename first (atomic if supported)
    if (LittleFS.rename(oldPath, newPath)) {
        LW_LOGI("Preset renamed: %s -> %s", sanitizedOldName.c_str(), sanitizedNewName.c_str());
        return true;
    }

    // Fallback: read old file, write new file, delete old
    File oldFile = LittleFS.open(oldPath, "r");
    if (!oldFile) {
        LW_LOGE("Failed to open preset for rename: %s", oldPath.c_str());
        return false;
    }

    File newFile = LittleFS.open(newPath, "w");
    if (!newFile) {
        oldFile.close();
        LW_LOGE("Failed to create new preset file: %s", newPath.c_str());
        return false;
    }

    // Copy file contents (bounded by file size)
    size_t bytesRead = 0;
    uint8_t buffer[256];
    while (oldFile.available() && bytesRead < 4096) {  // Limit to 4KB max
        size_t toRead = oldFile.read(buffer, sizeof(buffer));
        if (toRead == 0) break;
        newFile.write(buffer, toRead);
        bytesRead += toRead;
    }

    oldFile.close();
    newFile.close();

    if (bytesRead == 0 || bytesRead >= 4096) {
        // Copy failed or file too large
        LittleFS.remove(newPath);
        LW_LOGE("Failed to copy preset file during rename");
        return false;
    }

    // Delete old file
    if (!LittleFS.remove(oldPath)) {
        LW_LOGW("Renamed preset but failed to delete old file: %s", oldPath.c_str());
        // Continue anyway - rename succeeded even if cleanup failed
    }

    LW_LOGI("Preset renamed (via copy): %s -> %s", sanitizedOldName.c_str(), sanitizedNewName.c_str());
    return true;
}

std::vector<String> PresetManager::listPresets() {
    std::vector<String> presets;

    if (!m_initialized && !init()) {
        return presets;
    }

    if (!LittleFS.exists(PRESETS_DIR)) {
        return presets;
    }

    File dir = LittleFS.open(PRESETS_DIR);
    if (!dir || !dir.isDirectory()) {
        return presets;
    }

    File file = dir.openNextFile();
    while (file) {
        String filename = file.name();
        // Extract just the filename (remove path)
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) {
            filename = filename.substring(lastSlash + 1);
        }
        
        // Remove .json extension
        if (filename.endsWith(PRESET_EXT)) {
            filename = filename.substring(0, filename.length() - strlen(PRESET_EXT));
            presets.push_back(filename);
        }

        file = dir.openNextFile();
    }
    dir.close();

    return presets;
}

bool PresetManager::getPresetMetadata(const char* name, PresetMetadata& metadata) {
    if (!m_initialized && !init()) {
        return false;
    }

    String sanitizedName = sanitizeName(name);
    String filepath = getPresetPath(sanitizedName.c_str());

    if (!LittleFS.exists(filepath)) {
        return false;
    }

    File file = LittleFS.open(filepath, "r");
    if (!file) {
        return false;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return false;
    }

    metadata.name = doc["name"] | sanitizedName;
    metadata.description = doc["description"] | "";
    metadata.author = doc["author"] | "";
    metadata.created = doc["created"] | "";
    metadata.version = doc["version"] | 0;

    return true;
}

bool PresetManager::presetExists(const char* name) {
    if (!m_initialized && !init()) {
        return false;
    }

    String sanitizedName = sanitizeName(name);
    String filepath = getPresetPath(sanitizedName.c_str());
    return LittleFS.exists(filepath);
}

// ==================== JSON Export/Import ====================

bool PresetManager::exportToJson(const ZoneConfigData& config, JsonDocument& doc,
                                  const PresetMetadata* metadata) {
    // Add metadata if provided
    if (metadata) {
        doc["name"] = metadata->name;
        if (metadata->description.length() > 0) {
            doc["description"] = metadata->description;
        }
        if (metadata->author.length() > 0) {
            doc["author"] = metadata->author;
        }
        if (metadata->created.length() > 0) {
            doc["created"] = metadata->created;
        }
        doc["version"] = metadata->version;
    }

    // Zone configuration
    doc["zoneCount"] = config.zoneCount;
    doc["systemEnabled"] = config.systemEnabled;

    // Segments array
    JsonArray segmentsArray = doc["segments"].to<JsonArray>();
    for (uint8_t i = 0; i < config.zoneCount; i++) {
        JsonObject seg = segmentsArray.add<JsonObject>();
        seg["zoneId"] = config.segments[i].zoneId;
        seg["s1LeftStart"] = config.segments[i].s1LeftStart;
        seg["s1LeftEnd"] = config.segments[i].s1LeftEnd;
        seg["s1RightStart"] = config.segments[i].s1RightStart;
        seg["s1RightEnd"] = config.segments[i].s1RightEnd;
        seg["totalLeds"] = config.segments[i].totalLeds;
    }

    // Zones array
    JsonArray zonesArray = doc["zones"].to<JsonArray>();
    for (uint8_t i = 0; i < config.zoneCount; i++) {
        JsonObject zone = zonesArray.add<JsonObject>();
        zone["id"] = i;
        zone["effectId"] = config.zoneEffects[i];
        zone["enabled"] = config.zoneEnabled[i];
        zone["brightness"] = config.zoneBrightness[i];
        zone["speed"] = config.zoneSpeed[i];
        zone["paletteId"] = config.zonePalette[i];
        zone["blendMode"] = config.zoneBlendMode[i];
    }

    return true;
}

bool PresetManager::importFromJson(const JsonDocument& doc, ZoneConfigData& config,
                                    PresetMetadata* metadata) {
    // Extract metadata if requested
    if (metadata) {
        if (doc.containsKey("name")) {
            metadata->name = doc["name"].as<String>();
        }
        if (doc.containsKey("description")) {
            metadata->description = doc["description"].as<String>();
        }
        if (doc.containsKey("author")) {
            metadata->author = doc["author"].as<String>();
        }
        if (doc.containsKey("created")) {
            metadata->created = doc["created"].as<String>();
        }
        metadata->version = doc["version"] | 0;
    }

    // Zone configuration
    config.version = 2; // Current format version
    config.zoneCount = doc["zoneCount"] | 0;
    config.systemEnabled = doc["systemEnabled"] | false;

    // Validate zone count
    if (config.zoneCount == 0 || config.zoneCount > MAX_ZONES) {
        LW_LOGE("Invalid zone count: %d", config.zoneCount);
        return false;
    }

    // Import segments
    if (!doc.containsKey("segments") || !doc["segments"].is<JsonArray>()) {
        LW_LOGE("Missing or invalid segments array");
        return false;
    }

    JsonVariantConst segmentsVar = doc["segments"];
    uint8_t segCount = segmentsVar.size();
    if (segCount > MAX_ZONES) {
        segCount = MAX_ZONES;
    }

    for (uint8_t i = 0; i < segCount; i++) {
        JsonVariantConst segVar = segmentsVar[i];
        JsonObjectConst seg = segVar.as<JsonObjectConst>();
        config.segments[i].zoneId = seg["zoneId"] | i;
        config.segments[i].s1LeftStart = seg["s1LeftStart"] | 0;
        config.segments[i].s1LeftEnd = seg["s1LeftEnd"] | 0;
        config.segments[i].s1RightStart = seg["s1RightStart"] | 0;
        config.segments[i].s1RightEnd = seg["s1RightEnd"] | 0;
        config.segments[i].totalLeds = seg["totalLeds"] | 0;
    }

    // Import zones
    if (!doc.containsKey("zones") || !doc["zones"].is<JsonArray>()) {
        LW_LOGE("Missing or invalid zones array");
        return false;
    }

    JsonVariantConst zonesVar = doc["zones"];
    uint8_t zoneCount = zonesVar.size();
    if (zoneCount > MAX_ZONES) {
        zoneCount = MAX_ZONES;
    }

    for (uint8_t i = 0; i < zoneCount; i++) {
        JsonVariantConst zoneVar = zonesVar[i];
        JsonObjectConst zone = zoneVar.as<JsonObjectConst>();
        config.zoneEffects[i] = zone["effectId"] | 0;
        config.zoneEnabled[i] = zone["enabled"] | false;
        config.zoneBrightness[i] = zone["brightness"] | 255;
        config.zoneSpeed[i] = zone["speed"] | 1;
        config.zonePalette[i] = zone["paletteId"] | 0;
        config.zoneBlendMode[i] = zone["blendMode"] | 0;
    }

    // Calculate checksum
    config.calculateChecksum();

    return true;
}

// ==================== Utility Functions ====================

String PresetManager::getPresetPath(const char* name) {
    String sanitizedName = sanitizeName(name);
    return String(PRESETS_DIR) + "/" + sanitizedName + PRESET_EXT;
}

String PresetManager::sanitizeName(const char* name) {
    String sanitized;
    sanitized.reserve(MAX_NAME_LENGTH);

    for (size_t i = 0; i < strlen(name) && sanitized.length() < MAX_NAME_LENGTH; i++) {
        char c = name[i];
        // Allow alphanumeric, dash, underscore, and space (convert space to dash)
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            sanitized += c;
        } else if (c == ' ') {
            sanitized += '-';
        }
        // Skip all other characters
    }

    // Ensure not empty
    if (sanitized.length() == 0) {
        sanitized = "preset";
    }

    // Convert to lowercase
    sanitized.toLowerCase();

    return sanitized;
}

} // namespace persistence
} // namespace lightwaveos

