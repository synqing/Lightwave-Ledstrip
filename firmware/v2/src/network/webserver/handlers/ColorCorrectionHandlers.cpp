/**
 * @file ColorCorrectionHandlers.cpp
 * @brief REST API handlers for Color Correction Engine implementation
 */

#include "ColorCorrectionHandlers.h"
#include "effects/enhancement/ColorCorrectionEngine.h"
#include "effects/enhancement/ColorCorrectionPresets.h"
#include <ArduinoJson.h>

#define LW_LOG_TAG "CCHandlers"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

using namespace lightwaveos::enhancement;

void ColorCorrectionHandlers::handleGetConfig(AsyncWebServerRequest* request) {
    auto& engine = ColorCorrectionEngine::getInstance();
    const auto& config = engine.getConfig();

    sendSuccessResponse(request, [&config](JsonObject& data) {
        data["mode"] = static_cast<uint8_t>(config.mode);
        data["autoExposureEnabled"] = config.autoExposureEnabled;
        data["autoExposureTarget"] = config.autoExposureTarget;
        data["brownGuardrailEnabled"] = config.brownGuardrailEnabled;
        data["gammaEnabled"] = config.gammaEnabled;
        data["gammaValue"] = config.gammaValue;
        data["vClampEnabled"] = config.vClampEnabled;
        data["maxBrightness"] = config.maxBrightness;
        data["hsvMinSaturation"] = config.hsvMinSaturation;
        data["rgbWhiteThreshold"] = config.rgbWhiteThreshold;
        data["rgbTargetMin"] = config.rgbTargetMin;
        data["maxGreenPercentOfRed"] = config.maxGreenPercentOfRed;
        data["maxBluePercentOfRed"] = config.maxBluePercentOfRed;
        data["saturationBoostAmount"] = config.saturationBoostAmount;
    });
}

void ColorCorrectionHandlers::handleSetMode(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len
) {
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON,
                          "Invalid JSON payload");
        return;
    }

    if (!doc.containsKey("mode")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "Missing 'mode' field", "mode");
        return;
    }

    uint8_t modeValue = doc["mode"];
    if (modeValue > 3) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "Mode must be 0-3 (OFF=0, HSV=1, RGB=2, BOTH=3)", "mode");
        return;
    }

    auto& engine = ColorCorrectionEngine::getInstance();
    engine.setMode(static_cast<CorrectionMode>(modeValue));

    sendSuccessResponse(request, [modeValue](JsonObject& data) {
        data["mode"] = modeValue;

        // Include human-readable mode name
        const char* modeName = "OFF";
        switch (modeValue) {
            case 0: modeName = "OFF"; break;
            case 1: modeName = "HSV"; break;
            case 2: modeName = "RGB"; break;
            case 3: modeName = "BOTH"; break;
        }
        data["modeName"] = modeName;
    });

    LW_LOGI("Color correction mode set to: %d", modeValue);
}

void ColorCorrectionHandlers::handleSetConfig(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len
) {
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON,
                          "Invalid JSON payload");
        return;
    }

    auto& engine = ColorCorrectionEngine::getInstance();
    ColorCorrectionConfig& config = engine.getConfig();

    bool anyUpdated = false;

    // Update mode if provided
    if (doc.containsKey("mode")) {
        uint8_t modeValue = doc["mode"];
        if (modeValue > 3) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "Mode must be 0-3", "mode");
            return;
        }
        config.mode = static_cast<CorrectionMode>(modeValue);
        anyUpdated = true;
    }

    // Update auto-exposure parameters
    if (doc.containsKey("autoExposureEnabled")) {
        config.autoExposureEnabled = doc["autoExposureEnabled"];
        anyUpdated = true;
    }
    if (doc.containsKey("autoExposureTarget")) {
        config.autoExposureTarget = doc["autoExposureTarget"];
        anyUpdated = true;
    }

    // Update brown guardrail parameters
    if (doc.containsKey("brownGuardrailEnabled")) {
        config.brownGuardrailEnabled = doc["brownGuardrailEnabled"];
        anyUpdated = true;
    }
    if (doc.containsKey("maxGreenPercentOfRed")) {
        config.maxGreenPercentOfRed = doc["maxGreenPercentOfRed"];
        anyUpdated = true;
    }
    if (doc.containsKey("maxBluePercentOfRed")) {
        config.maxBluePercentOfRed = doc["maxBluePercentOfRed"];
        anyUpdated = true;
    }

    // Update gamma parameters
    if (doc.containsKey("gammaEnabled")) {
        config.gammaEnabled = doc["gammaEnabled"];
        anyUpdated = true;
    }
    if (doc.containsKey("gammaValue")) {
        float gammaValue = doc["gammaValue"];
        if (gammaValue < 1.0f || gammaValue > 3.0f) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "Gamma value must be 1.0-3.0", "gammaValue");
            return;
        }
        config.gammaValue = gammaValue;
        anyUpdated = true;
    }

    // Update V-clamping parameters
    if (doc.containsKey("vClampEnabled")) {
        config.vClampEnabled = doc["vClampEnabled"];
        anyUpdated = true;
    }
    if (doc.containsKey("maxBrightness")) {
        config.maxBrightness = doc["maxBrightness"];
        anyUpdated = true;
    }
    if (doc.containsKey("saturationBoostAmount")) {
        config.saturationBoostAmount = doc["saturationBoostAmount"];
        anyUpdated = true;
    }

    // Update HSV mode parameters
    if (doc.containsKey("hsvMinSaturation")) {
        config.hsvMinSaturation = doc["hsvMinSaturation"];
        anyUpdated = true;
    }

    // Update RGB mode parameters
    if (doc.containsKey("rgbWhiteThreshold")) {
        config.rgbWhiteThreshold = doc["rgbWhiteThreshold"];
        anyUpdated = true;
    }
    if (doc.containsKey("rgbTargetMin")) {
        config.rgbTargetMin = doc["rgbTargetMin"];
        anyUpdated = true;
    }

    if (!anyUpdated) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_PARAMETER,
                          "No valid parameters to update");
        return;
    }

    // Apply the updated config
    engine.setConfig(config);

    // Return the full updated config
    sendSuccessResponse(request, [&config](JsonObject& data) {
        data["updated"] = true;
        data["mode"] = static_cast<uint8_t>(config.mode);
        data["autoExposureEnabled"] = config.autoExposureEnabled;
        data["autoExposureTarget"] = config.autoExposureTarget;
        data["brownGuardrailEnabled"] = config.brownGuardrailEnabled;
        data["gammaEnabled"] = config.gammaEnabled;
        data["gammaValue"] = config.gammaValue;
        data["vClampEnabled"] = config.vClampEnabled;
        data["maxBrightness"] = config.maxBrightness;
        data["hsvMinSaturation"] = config.hsvMinSaturation;
        data["rgbWhiteThreshold"] = config.rgbWhiteThreshold;
        data["rgbTargetMin"] = config.rgbTargetMin;
        data["maxGreenPercentOfRed"] = config.maxGreenPercentOfRed;
        data["maxBluePercentOfRed"] = config.maxBluePercentOfRed;
        data["saturationBoostAmount"] = config.saturationBoostAmount;
    });

    LW_LOGI("Color correction config updated");
}

void ColorCorrectionHandlers::handleSave(AsyncWebServerRequest* request) {
    auto& engine = ColorCorrectionEngine::getInstance();
    engine.saveToNVS();

    sendSuccessResponse(request, [](JsonObject& data) {
        data["saved"] = true;
    });

    LW_LOGI("Color correction config saved to NVS");
}

void ColorCorrectionHandlers::handleGetPresets(AsyncWebServerRequest* request) {
    // Detect current preset based on config
    ColorCorrectionPreset currentPreset = detectCurrentPreset();

    sendSuccessResponse(request, [currentPreset](JsonObject& data) {
        // Build presets array
        JsonArray presets = data["presets"].to<JsonArray>();

        for (uint8_t i = 0; i < getPresetCount(); i++) {
            JsonObject preset = presets.add<JsonObject>();
            preset["id"] = i;
            preset["name"] = getPresetName(static_cast<ColorCorrectionPreset>(i));
        }

        // Include current detected preset
        data["currentPreset"] = static_cast<uint8_t>(currentPreset);
        data["currentPresetName"] = getPresetName(currentPreset);
    });
}

void ColorCorrectionHandlers::handleSetPreset(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len
) {
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON,
                          "Invalid JSON payload");
        return;
    }

    if (!doc.containsKey("preset")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "Missing 'preset' field", "preset");
        return;
    }

    uint8_t presetValue = doc["preset"];
    if (presetValue >= getPresetCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "Preset must be 0-3 (Off=0, Subtle=1, Balanced=2, Aggressive=3)", "preset");
        return;
    }

    // Check if we should save to NVS
    bool saveToNVS = doc["save"] | false;

    // Apply the preset
    ColorCorrectionPreset preset = static_cast<ColorCorrectionPreset>(presetValue);
    applyPreset(preset, saveToNVS);

    // Get the resulting config to return
    const auto& config = ColorCorrectionEngine::getInstance().getConfig();

    sendSuccessResponse(request, [preset, saveToNVS, &config](JsonObject& data) {
        data["preset"] = static_cast<uint8_t>(preset);
        data["presetName"] = getPresetName(preset);
        data["saved"] = saveToNVS;

        // Include full config for client sync
        JsonObject configObj = data["config"].to<JsonObject>();
        configObj["mode"] = static_cast<uint8_t>(config.mode);
        configObj["autoExposureEnabled"] = config.autoExposureEnabled;
        configObj["autoExposureTarget"] = config.autoExposureTarget;
        configObj["brownGuardrailEnabled"] = config.brownGuardrailEnabled;
        configObj["gammaEnabled"] = config.gammaEnabled;
        configObj["gammaValue"] = config.gammaValue;
        configObj["vClampEnabled"] = config.vClampEnabled;
        configObj["maxBrightness"] = config.maxBrightness;
        configObj["ditheringEnabled"] = config.ditheringEnabled;
        configObj["spectralCorrectionEnabled"] = config.spectralCorrectionEnabled;
        configObj["laceEnabled"] = config.laceEnabled;
    });

    LW_LOGI("Color correction preset applied: %s (save=%d)",
            getPresetName(preset), saveToNVS);
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
