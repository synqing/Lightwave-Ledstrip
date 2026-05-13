/**
 * @file ColorCorrectionHandlers.cpp
 * @brief Colour correction REST API handlers
 */

#include "ColorCorrectionHandlers.h"
#include "../../ApiResponse.h"
#include "../../../effects/enhancement/ColorCorrectionEngine.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

namespace {

using lightwaveos::enhancement::ColorCorrectionConfig;
using lightwaveos::enhancement::ColorCorrectionEngine;
using lightwaveos::enhancement::CorrectionMode;
using lightwaveos::enhancement::GammaLutStatus;

void appendGammaLutStatus(JsonObject& data, const GammaLutStatus& status) {
    data["lutGenerationId"] = status.lutGenerationId;
    JsonObject gammaLut = data["gammaLut"].to<JsonObject>();
    gammaLut["0"] = status.lut0;
    gammaLut["32"] = status.lut32;
    gammaLut["64"] = status.lut64;
    gammaLut["128"] = status.lut128;
    gammaLut["192"] = status.lut192;
    gammaLut["255"] = status.lut255;
}

void appendConfig(JsonObject& data,
                  const ColorCorrectionConfig& cfg,
                  const GammaLutStatus& gammaStatus) {
    data["mode"] = static_cast<uint8_t>(cfg.mode);
    data["modeNames"] = "OFF,HSV,RGB,BOTH";
    data["hsvMinSaturation"] = cfg.hsvMinSaturation;
    data["rgbWhiteThreshold"] = cfg.rgbWhiteThreshold;
    data["rgbTargetMin"] = cfg.rgbTargetMin;
    data["autoExposureEnabled"] = cfg.autoExposureEnabled;
    data["autoExposureTarget"] = cfg.autoExposureTarget;
    data["gammaEnabled"] = cfg.gammaEnabled;
    data["gammaValue"] = cfg.gammaValue;
    appendGammaLutStatus(data, gammaStatus);
    data["brownGuardrailEnabled"] = cfg.brownGuardrailEnabled;
    data["maxGreenPercentOfRed"] = cfg.maxGreenPercentOfRed;
    data["maxBluePercentOfRed"] = cfg.maxBluePercentOfRed;
    data["vClampEnabled"] = cfg.vClampEnabled;
    data["maxBrightness"] = cfg.maxBrightness;
    data["saturationBoostAmount"] = cfg.saturationBoostAmount;
}

bool parseJsonBody(AsyncWebServerRequest* request, uint8_t* body, size_t len, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, body, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, "Invalid JSON");
        return false;
    }
    return true;
}

} // namespace

void ColorCorrectionHandlers::handleGetConfig(AsyncWebServerRequest* request) {
    auto& engine = ColorCorrectionEngine::getInstance();
    const ColorCorrectionConfig cfg = engine.getConfig();
    const GammaLutStatus gammaStatus = engine.getGammaLutStatus();
    sendSuccessResponse(request, [cfg, gammaStatus](JsonObject& data) {
        appendConfig(data, cfg, gammaStatus);
    });
}

void ColorCorrectionHandlers::handleSetMode(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (!parseJsonBody(request, data, len, doc)) return;

    JsonObjectConst root = doc.as<JsonObjectConst>();
    if (!root["mode"].is<uint8_t>()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::MISSING_FIELD, "mode is required", "mode");
        return;
    }

    uint8_t mode = root["mode"].as<uint8_t>();
    if (mode > 3) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::OUT_OF_RANGE, "mode must be 0-3", "mode");
        return;
    }

    auto& engine = ColorCorrectionEngine::getInstance();
    engine.setMode(static_cast<CorrectionMode>(mode));
    const ColorCorrectionConfig cfg = engine.getConfig();
    const GammaLutStatus gammaStatus = engine.getGammaLutStatus();
    sendSuccessResponse(request, [cfg, gammaStatus](JsonObject& responseData) {
        appendConfig(responseData, cfg, gammaStatus);
    });
}

void ColorCorrectionHandlers::handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (!parseJsonBody(request, data, len, doc)) return;

    JsonObjectConst root = doc.as<JsonObjectConst>();
    auto& engine = ColorCorrectionEngine::getInstance();
    ColorCorrectionConfig cfg = engine.getConfig();

    if (root["mode"].is<uint8_t>()) {
        uint8_t mode = root["mode"].as<uint8_t>();
        if (mode > 3) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::OUT_OF_RANGE, "mode must be 0-3", "mode");
            return;
        }
        cfg.mode = static_cast<CorrectionMode>(mode);
    }
    if (root["hsvMinSaturation"].is<uint8_t>()) cfg.hsvMinSaturation = root["hsvMinSaturation"].as<uint8_t>();
    if (root["rgbWhiteThreshold"].is<uint8_t>()) cfg.rgbWhiteThreshold = root["rgbWhiteThreshold"].as<uint8_t>();
    if (root["rgbTargetMin"].is<uint8_t>()) cfg.rgbTargetMin = root["rgbTargetMin"].as<uint8_t>();
    if (root["autoExposureEnabled"].is<bool>()) cfg.autoExposureEnabled = root["autoExposureEnabled"].as<bool>();
    if (root["autoExposureTarget"].is<uint8_t>()) cfg.autoExposureTarget = root["autoExposureTarget"].as<uint8_t>();
    if (root["gammaEnabled"].is<bool>()) cfg.gammaEnabled = root["gammaEnabled"].as<bool>();
    if (root["gammaValue"].is<float>()) {
        float gammaValue = root["gammaValue"].as<float>();
        if (gammaValue < 1.0f || gammaValue > 3.0f) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::OUT_OF_RANGE,
                              "gammaValue must be 1.0-3.0", "gammaValue");
            return;
        }
        cfg.gammaValue = gammaValue;
    }
    if (root["brownGuardrailEnabled"].is<bool>()) cfg.brownGuardrailEnabled = root["brownGuardrailEnabled"].as<bool>();
    if (root["maxGreenPercentOfRed"].is<uint8_t>()) cfg.maxGreenPercentOfRed = root["maxGreenPercentOfRed"].as<uint8_t>();
    if (root["maxBluePercentOfRed"].is<uint8_t>()) cfg.maxBluePercentOfRed = root["maxBluePercentOfRed"].as<uint8_t>();
    if (root["vClampEnabled"].is<bool>()) cfg.vClampEnabled = root["vClampEnabled"].as<bool>();
    if (root["maxBrightness"].is<uint8_t>()) cfg.maxBrightness = root["maxBrightness"].as<uint8_t>();
    if (root["saturationBoostAmount"].is<uint8_t>()) {
        cfg.saturationBoostAmount = root["saturationBoostAmount"].as<uint8_t>();
    }

    engine.setConfig(cfg);
    const GammaLutStatus gammaStatus = engine.getGammaLutStatus();
    sendSuccessResponse(request, [cfg, gammaStatus](JsonObject& responseData) {
        appendConfig(responseData, cfg, gammaStatus);
        responseData["updated"] = true;
    });
}

void ColorCorrectionHandlers::handleSave(AsyncWebServerRequest* request) {
    auto& engine = ColorCorrectionEngine::getInstance();
    engine.saveToNVS();
    const GammaLutStatus gammaStatus = engine.getGammaLutStatus();
    sendSuccessResponse(request, [gammaStatus](JsonObject& data) {
        data["saved"] = true;
        appendGammaLutStatus(data, gammaStatus);
    });
}

void ColorCorrectionHandlers::handleGetPresets(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["presets"] = JsonArray();
        data["count"] = 0;
        data["status"] = "stub";
    });
}

void ColorCorrectionHandlers::handleSetPreset(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    (void)data; (void)len;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Color correction not yet implemented");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
