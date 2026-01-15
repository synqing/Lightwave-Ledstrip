#include "PresetHandlers.h"
#include "../../RequestValidator.h"
#include <LittleFS.h>
#include <Arduino.h>
#define LW_LOG_TAG "PresetHandlers"
#include "../../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// ==================== List Presets ====================

void PresetHandlers::handleList(AsyncWebServerRequest* request,
                                 lightwaveos::persistence::PresetManager* presetManager) {
    if (!presetManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Preset manager not available");
        return;
    }

    if (!presetManager->init()) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to initialize preset manager");
        return;
    }

    std::vector<String> presets = presetManager->listPresets();

    sendSuccessResponse(request, [&presets, presetManager](JsonObject& data) {
        JsonArray presetsArray = data["presets"].to<JsonArray>();
        for (const String& name : presets) {
            JsonObject preset = presetsArray.add<JsonObject>();
            preset["name"] = name;
            
            // Get metadata
            lightwaveos::persistence::PresetMetadata metadata;
            if (presetManager->getPresetMetadata(name.c_str(), metadata)) {
                if (metadata.description.length() > 0) {
                    preset["description"] = metadata.description;
                }
                if (metadata.author.length() > 0) {
                    preset["author"] = metadata.author;
                }
                if (metadata.created.length() > 0) {
                    preset["created"] = metadata.created;
                }
            }
        }
        data["count"] = presets.size();
    });
}

// ==================== Get Preset (Download) ====================

void PresetHandlers::handleGet(AsyncWebServerRequest* request,
                                lightwaveos::persistence::PresetManager* presetManager) {
    if (!presetManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Preset manager not available");
        return;
    }

    String presetName = extractPresetNameFromPath(request);
    if (presetName.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Preset name required");
        return;
    }

    if (!presetManager->presetExists(presetName.c_str())) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found");
        return;
    }

    // Load preset and convert to JSON
    lightwaveos::persistence::ZoneConfigData config;
    if (!presetManager->loadPreset(presetName.c_str(), config)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to load preset");
        return;
    }

    // Get metadata
    lightwaveos::persistence::PresetMetadata metadata;
    presetManager->getPresetMetadata(presetName.c_str(), metadata);

    // Export to JSON
    StaticJsonDocument<2048> doc;
    if (!lightwaveos::persistence::PresetManager::exportToJson(config, doc, &metadata)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to export preset");
        return;
    }

    // Send as JSON file with proper headers for download
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    
    // Set Content-Disposition header for file download
    AsyncWebServerResponse* response = request->beginResponse(HttpStatus::OK, "application/json", jsonOutput);
    response->addHeader("Content-Disposition", String("attachment; filename=\"") + presetName + ".json\"");
    request->send(response);
}

// ==================== Save Preset ====================

void PresetHandlers::handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                 lightwaveos::persistence::PresetManager* presetManager) {
    if (!presetManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Preset manager not available");
        return;
    }

    if (!presetManager->init()) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to initialize preset manager");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }

    // Extract preset name (required)
    if (!doc.containsKey("name") || !doc["name"].is<const char*>()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Preset name required");
        return;
    }

    String presetName = doc["name"].as<String>();
    if (presetName.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Preset name cannot be empty");
        return;
    }

    // Check if preset already exists
    if (presetManager->presetExists(presetName.c_str())) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Preset already exists");
        return;
    }

    // Import zone configuration from JSON
    lightwaveos::persistence::ZoneConfigData config;
    lightwaveos::persistence::PresetMetadata metadata;
    if (!lightwaveos::persistence::PresetManager::importFromJson(doc, config, &metadata)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Invalid preset configuration");
        return;
    }

    // Extract description and author if provided
    const char* description = doc.containsKey("description") ? doc["description"].as<const char*>() : nullptr;
    const char* author = doc.containsKey("author") ? doc["author"].as<const char*>() : nullptr;

    // Save preset
    if (!presetManager->savePreset(presetName.c_str(), config, description, author)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to save preset");
        return;
    }

    sendSuccessResponse(request, [presetName](JsonObject& data) {
        data["saved"] = true;
        data["name"] = presetName;
        data["message"] = "Preset saved successfully";
    });
}

// ==================== Update Preset ====================

void PresetHandlers::handleUpdate(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                   lightwaveos::persistence::PresetManager* presetManager) {
    if (!presetManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Preset manager not available");
        return;
    }

    String presetName = extractPresetNameFromPath(request);
    if (presetName.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Preset name required");
        return;
    }

    if (!presetManager->presetExists(presetName.c_str())) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }

    // Import zone configuration from JSON
    lightwaveos::persistence::ZoneConfigData config;
    lightwaveos::persistence::PresetMetadata metadata;
    if (!lightwaveos::persistence::PresetManager::importFromJson(doc, config, &metadata)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Invalid preset configuration");
        return;
    }

    // Extract description and author if provided
    const char* description = doc.containsKey("description") ? doc["description"].as<const char*>() : nullptr;
    const char* author = doc.containsKey("author") ? doc["author"].as<const char*>() : nullptr;

    // Delete old preset and save new one
    presetManager->deletePreset(presetName.c_str());
    if (!presetManager->savePreset(presetName.c_str(), config, description, author)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to update preset");
        return;
    }

    sendSuccessResponse(request, [presetName](JsonObject& data) {
        data["updated"] = true;
        data["name"] = presetName;
        data["message"] = "Preset updated successfully";
    });
}

// ==================== Delete Preset ====================

void PresetHandlers::handleDelete(AsyncWebServerRequest* request,
                                   lightwaveos::persistence::PresetManager* presetManager) {
    if (!presetManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Preset manager not available");
        return;
    }

    String presetName = extractPresetNameFromPath(request);
    if (presetName.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Preset name required");
        return;
    }

    if (!presetManager->presetExists(presetName.c_str())) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found");
        return;
    }

    if (!presetManager->deletePreset(presetName.c_str())) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to delete preset");
        return;
    }

    sendSuccessResponse(request, [presetName](JsonObject& data) {
        data["deleted"] = true;
        data["name"] = presetName;
        data["message"] = "Preset deleted successfully";
    });
}

// ==================== Rename Preset ====================

void PresetHandlers::handleRename(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                   lightwaveos::persistence::PresetManager* presetManager) {
    if (!presetManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Preset manager not available");
        return;
    }

    String presetName = extractPresetNameFromPath(request);
    if (presetName.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Preset name required");
        return;
    }

    if (!presetManager->presetExists(presetName.c_str())) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }

    // Extract new name (required)
    if (!doc.containsKey("newName") || !doc["newName"].is<const char*>()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "newName field required");
        return;
    }

    String newName = doc["newName"].as<String>();
    if (newName.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "New preset name cannot be empty");
        return;
    }

    // Rename preset
    if (!presetManager->renamePreset(presetName.c_str(), newName.c_str())) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to rename preset");
        return;
    }

    sendSuccessResponse(request, [presetName, newName](JsonObject& data) {
        data["renamed"] = true;
        data["oldName"] = presetName;
        data["newName"] = newName;
        data["message"] = "Preset renamed successfully";
    });
}

// ==================== Load Preset ====================

void PresetHandlers::handleLoad(AsyncWebServerRequest* request,
                                 lightwaveos::zones::ZoneComposer* composer,
                                 lightwaveos::persistence::ZoneConfigManager* configManager,
                                 lightwaveos::persistence::PresetManager* presetManager,
                                 std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!configManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone config manager not available");
        return;
    }

    if (!presetManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Preset manager not available");
        return;
    }

    String presetName = extractPresetNameFromPath(request);
    if (presetName.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Preset name required");
        return;
    }

    // Load preset
    lightwaveos::persistence::ZoneConfigData config;
    if (!presetManager->loadPreset(presetName.c_str(), config)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found or invalid");
        return;
    }

    // Apply configuration
    configManager->importConfig(config);

    // Broadcast updated zone state
    if (broadcastZoneState) {
        broadcastZoneState();
    }

    sendSuccessResponse(request, [composer, presetName](JsonObject& data) {
        data["loaded"] = true;
        data["name"] = presetName;
        data["message"] = "Preset loaded successfully";
        data["enabled"] = composer->isEnabled();
        data["zoneCount"] = composer->getZoneCount();
    });
}

// ==================== Save Current Config ====================

void PresetHandlers::handleSaveCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                       lightwaveos::zones::ZoneComposer* composer,
                                       lightwaveos::persistence::ZoneConfigManager* configManager,
                                       lightwaveos::persistence::PresetManager* presetManager) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!configManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone config manager not available");
        return;
    }

    if (!presetManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Preset manager not available");
        return;
    }

    if (!presetManager->init()) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to initialize preset manager");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }

    // Extract preset name (required)
    if (!doc.containsKey("name") || !doc["name"].is<const char*>()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Preset name required");
        return;
    }

    String presetName = doc["name"].as<String>();
    if (presetName.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Preset name cannot be empty");
        return;
    }

    // Export current configuration
    lightwaveos::persistence::ZoneConfigData config;
    configManager->exportConfig(config);

    // Extract description and author if provided
    const char* description = doc.containsKey("description") ? doc["description"].as<const char*>() : nullptr;
    const char* author = doc.containsKey("author") ? doc["author"].as<const char*>() : nullptr;

    // Save preset
    if (!presetManager->savePreset(presetName.c_str(), config, description, author)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to save preset");
        return;
    }

    sendSuccessResponse(request, [presetName](JsonObject& data) {
        data["saved"] = true;
        data["name"] = presetName;
        data["message"] = "Current configuration saved as preset";
    });
}

// ==================== Utility Functions ====================

String PresetHandlers::extractPresetNameFromPath(AsyncWebServerRequest* request) {
    String path = request->url();
    
    // Extract name from paths like:
    // /api/v1/presets/my-preset
    // /api/v1/presets/my-preset/load
    
    int presetsPos = path.indexOf("/presets/");
    if (presetsPos < 0) {
        return "";
    }
    
    int nameStart = presetsPos + 9; // Length of "/presets/"
    int nameEnd = path.indexOf("/", nameStart);
    if (nameEnd < 0) {
        nameEnd = path.length();
    }
    
    String name = path.substring(nameStart, nameEnd);
    
    // Remove .json extension if present
    if (name.endsWith(".json")) {
        name = name.substring(0, name.length() - 5);
    }
    
    return name;
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

