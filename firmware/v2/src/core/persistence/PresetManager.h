/**
 * @file PresetManager.h
 * @brief File-based preset library manager for zone configurations
 *
 * LightwaveOS v2 - Preset Library System
 *
 * Manages user-created zone presets stored as JSON files on LittleFS.
 * Provides full CRUD operations and export/import functionality.
 *
 * Features:
 * - Named presets (user-defined names)
 * - Multiple presets (limited only by flash space)
 * - JSON format for human-readable, shareable files
 * - Download/upload capability
 * - Metadata support (name, description, author, created date)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ZoneConfigManager.h"

namespace lightwaveos {
namespace persistence {

using namespace lightwaveos::zones;

// ==================== Preset Metadata ====================

/**
 * @brief Preset file structure (JSON format)
 */
struct PresetMetadata {
    String name;              // Preset name (used as filename)
    String description;       // Optional description
    String author;            // Optional author name
    String created;           // ISO 8601 timestamp
    uint8_t version;         // Format version
};

// ==================== PresetManager Class ====================

/**
 * @brief Manages zone preset library on LittleFS
 *
 * Usage:
 *   PresetManager presetMgr;
 *   presetMgr.init();
 *
 *   // Save current config as preset
 *   ZoneConfigData config;
 *   zoneConfigMgr->exportConfig(config);
 *   presetMgr.savePreset("my-preset", config, "My awesome preset", "User");
 *
 *   // List all presets
 *   std::vector<String> presets = presetMgr.listPresets();
 *
 *   // Load a preset
 *   ZoneConfigData loaded;
 *   presetMgr.loadPreset("my-preset", loaded);
 */
class PresetManager {
public:
    /**
     * @brief Constructor
     */
    PresetManager();

    /**
     * @brief Initialize preset directory (creates /presets/ if needed)
     * @return true if initialized successfully
     */
    bool init();

    /**
     * @brief Save zone configuration as a named preset
     * @param name Preset name (will be sanitized for filename)
     * @param config Zone configuration to save
     * @param description Optional description
     * @param author Optional author name
     * @return true if saved successfully
     */
    bool savePreset(const char* name, const ZoneConfigData& config,
                    const char* description = nullptr,
                    const char* author = nullptr);

    /**
     * @brief Load a preset by name
     * @param name Preset name
     * @param config Output: loaded configuration
     * @return true if loaded successfully
     */
    bool loadPreset(const char* name, ZoneConfigData& config);

    /**
     * @brief Delete a preset
     * @param name Preset name
     * @return true if deleted successfully
     */
    bool deletePreset(const char* name);

    /**
     * @brief Rename a preset
     * @param oldName Current preset name
     * @param newName New preset name (will be sanitized)
     * @return true if renamed successfully
     */
    bool renamePreset(const char* oldName, const char* newName);

    /**
     * @brief List all saved presets
     * @return Vector of preset names
     */
    std::vector<String> listPresets();

    /**
     * @brief Get preset metadata
     * @param name Preset name
     * @param metadata Output: preset metadata
     * @return true if metadata retrieved successfully
     */
    bool getPresetMetadata(const char* name, PresetMetadata& metadata);

    /**
     * @brief Check if preset exists
     * @param name Preset name
     * @return true if preset exists
     */
    bool presetExists(const char* name);

    /**
     * @brief Export zone config to JSON document
     * @param config Zone configuration
     * @param doc JSON document to populate
     * @param metadata Optional metadata to include
     * @return true if exported successfully
     */
    static bool exportToJson(const ZoneConfigData& config, JsonDocument& doc,
                             const PresetMetadata* metadata = nullptr);

    /**
     * @brief Import zone config from JSON document
     * @param doc JSON document
     * @param config Output: zone configuration
     * @param metadata Output: preset metadata (optional)
     * @return true if imported successfully
     */
    static bool importFromJson(const JsonDocument& doc, ZoneConfigData& config,
                               PresetMetadata* metadata = nullptr);

    /**
     * @brief Get the full path for a preset file
     * @param name Preset name
     * @return Full file path
     */
    static String getPresetPath(const char* name);

    /**
     * @brief Sanitize preset name for use as filename
     * @param name Original name
     * @return Sanitized filename-safe name
     */
    static String sanitizeName(const char* name);

private:
    static constexpr const char* PRESETS_DIR = "/presets";
    static constexpr const char* PRESET_EXT = ".json";
    static constexpr size_t MAX_NAME_LENGTH = 64;
    static constexpr size_t MAX_DESCRIPTION_LENGTH = 256;
    static constexpr uint8_t PRESET_FORMAT_VERSION = 1;

    bool m_initialized;

    /**
     * @brief Ensure presets directory exists
     * @return true if directory exists or was created
     */
    bool ensurePresetsDir();
};

} // namespace persistence
} // namespace lightwaveos

