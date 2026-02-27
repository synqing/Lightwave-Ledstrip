/**
 * @file WsColorCodec.cpp
 * @brief WebSocket color codec implementation
 *
 * Single canonical JSON parser for color WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsColorCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Group B: Simple Setters
// ============================================================================

ColorEnableBlendDecodeResult WsColorCodec::decodeEnableBlend(JsonObjectConst root) {
    ColorEnableBlendDecodeResult result;
    result.request = ColorEnableBlendRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract enable (required bool)
    if (!root.containsKey("enable")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'enable'");
        return result;
    }
    if (!root["enable"].is<bool>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'enable' must be a boolean");
        return result;
    }
    result.request.enable = root["enable"].as<bool>();

    result.success = true;
    return result;
}

ColorEnableRotationDecodeResult WsColorCodec::decodeEnableRotation(JsonObjectConst root) {
    ColorEnableRotationDecodeResult result;
    result.request = ColorEnableRotationRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract enable (required bool)
    if (!root.containsKey("enable")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'enable'");
        return result;
    }
    if (!root["enable"].is<bool>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'enable' must be a boolean");
        return result;
    }
    result.request.enable = root["enable"].as<bool>();

    result.success = true;
    return result;
}

ColorEnableDiffusionDecodeResult WsColorCodec::decodeEnableDiffusion(JsonObjectConst root) {
    ColorEnableDiffusionDecodeResult result;
    result.request = ColorEnableDiffusionRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract enable (required bool)
    if (!root.containsKey("enable")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'enable'");
        return result;
    }
    if (!root["enable"].is<bool>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'enable' must be a boolean");
        return result;
    }
    result.request.enable = root["enable"].as<bool>();

    result.success = true;
    return result;
}

ColorSetDiffusionAmountDecodeResult WsColorCodec::decodeSetDiffusionAmount(JsonObjectConst root) {
    ColorSetDiffusionAmountDecodeResult result;
    result.request = ColorSetDiffusionAmountRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract amount (required, 0-255)
    if (!root.containsKey("amount")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'amount'");
        return result;
    }
    if (!root["amount"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'amount' must be an integer");
        return result;
    }
    int amount = root["amount"].as<int>();
    if (amount < 0 || amount > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "amount out of range (0-255): %d", amount);
        return result;
    }
    result.request.amount = static_cast<uint8_t>(amount);

    result.success = true;
    return result;
}

ColorCorrectionSetModeDecodeResult WsColorCodec::decodeSetMode(JsonObjectConst root) {
    ColorCorrectionSetModeDecodeResult result;
    result.request = ColorCorrectionSetModeRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract mode (required, 0-3)
    if (!root.containsKey("mode")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'mode'");
        return result;
    }
    if (!root["mode"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'mode' must be an integer");
        return result;
    }
    int mode = root["mode"].as<int>();
    if (mode < 0 || mode > 3) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "mode out of range (0-3): %d", mode);
        return result;
    }
    result.request.mode = static_cast<uint8_t>(mode);

    result.success = true;
    return result;
}

ColorSetRotationSpeedDecodeResult WsColorCodec::decodeSetRotationSpeed(JsonObjectConst root) {
    ColorSetRotationSpeedDecodeResult result;
    result.request = ColorSetRotationSpeedRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract degreesPerFrame (required float)
    if (!root.containsKey("degreesPerFrame")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'degreesPerFrame'");
        return result;
    }
    if (!root["degreesPerFrame"].is<float>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'degreesPerFrame' must be a float");
        return result;
    }
    result.request.degreesPerFrame = root["degreesPerFrame"].as<float>();

    result.success = true;
    return result;
}

// ============================================================================
// Group C: Multi-Field
// ============================================================================

ColorSetBlendPalettesDecodeResult WsColorCodec::decodeSetBlendPalettes(JsonObjectConst root) {
    ColorSetBlendPalettesDecodeResult result;
    result.request = ColorSetBlendPalettesRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract palette1 (required)
    if (!root.containsKey("palette1") || !root.containsKey("palette2")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'palette1' or 'palette2'");
        return result;
    }
    if (!root["palette1"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'palette1' must be an integer");
        return result;
    }
    if (!root["palette2"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'palette2' must be an integer");
        return result;
    }
    int p1 = root["palette1"].as<int>();
    int p2 = root["palette2"].as<int>();
    if (p1 < 0) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "palette1 must be >= 0: %d", p1);
        return result;
    }
    if (p2 < 0) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "palette2 must be >= 0: %d", p2);
        return result;
    }
    result.request.palette1 = static_cast<uint8_t>(p1);
    result.request.palette2 = static_cast<uint8_t>(p2);

    // Extract palette3 (optional, 255 means none)
    if (root.containsKey("palette3") && root["palette3"].is<int>()) {
        int p3 = root["palette3"].as<int>();
        if (p3 < 0) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "palette3 must be >= 0: %d", p3);
            return result;
        }
        result.request.palette3 = static_cast<uint8_t>(p3);
    } else {
        result.request.palette3 = 255; // Default: none
    }

    result.success = true;
    return result;
}

ColorSetBlendFactorsDecodeResult WsColorCodec::decodeSetBlendFactors(JsonObjectConst root) {
    ColorSetBlendFactorsDecodeResult result;
    result.request = ColorSetBlendFactorsRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract factor1 and factor2 (required)
    if (!root.containsKey("factor1") || !root.containsKey("factor2")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'factor1' or 'factor2'");
        return result;
    }
    if (!root["factor1"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'factor1' must be an integer");
        return result;
    }
    if (!root["factor2"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'factor2' must be an integer");
        return result;
    }
    int f1 = root["factor1"].as<int>();
    int f2 = root["factor2"].as<int>();
    if (f1 < 0 || f1 > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "factor1 out of range (0-255): %d", f1);
        return result;
    }
    if (f2 < 0 || f2 > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "factor2 out of range (0-255): %d", f2);
        return result;
    }
    result.request.factor1 = static_cast<uint8_t>(f1);
    result.request.factor2 = static_cast<uint8_t>(f2);

    // Extract factor3 (optional, default: 0)
    if (root.containsKey("factor3") && root["factor3"].is<int>()) {
        int f3 = root["factor3"].as<int>();
        if (f3 < 0 || f3 > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "factor3 out of range (0-255): %d", f3);
            return result;
        }
        result.request.factor3 = static_cast<uint8_t>(f3);
    } else {
        result.request.factor3 = 0; // Default: 0
    }

    result.success = true;
    return result;
}

// ============================================================================
// Group D: Complex Optional Fields
// ============================================================================

ColorCorrectionSetConfigDecodeResult WsColorCodec::decodeSetConfig(JsonObjectConst root) {
    ColorCorrectionSetConfigDecodeResult result;
    result.request = ColorCorrectionSetConfigRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract mode (optional, 0-3)
    if (root.containsKey("mode") && root["mode"].is<int>()) {
        int mode = root["mode"].as<int>();
        if (mode < 0 || mode > 3) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "mode out of range (0-3): %d", mode);
            return result;
        }
        result.request.mode = static_cast<uint8_t>(mode);
        result.request.hasMode = true;
    }

    // Extract hsvMinSaturation (optional, 0-255)
    if (root.containsKey("hsvMinSaturation") && root["hsvMinSaturation"].is<int>()) {
        int value = root["hsvMinSaturation"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "hsvMinSaturation out of range (0-255): %d", value);
            return result;
        }
        result.request.hsvMinSaturation = static_cast<uint8_t>(value);
        result.request.hasHsvMinSaturation = true;
    }

    // Extract rgbWhiteThreshold (optional, 0-255)
    if (root.containsKey("rgbWhiteThreshold") && root["rgbWhiteThreshold"].is<int>()) {
        int value = root["rgbWhiteThreshold"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "rgbWhiteThreshold out of range (0-255): %d", value);
            return result;
        }
        result.request.rgbWhiteThreshold = static_cast<uint8_t>(value);
        result.request.hasRgbWhiteThreshold = true;
    }

    // Extract rgbTargetMin (optional, 0-255)
    if (root.containsKey("rgbTargetMin") && root["rgbTargetMin"].is<int>()) {
        int value = root["rgbTargetMin"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "rgbTargetMin out of range (0-255): %d", value);
            return result;
        }
        result.request.rgbTargetMin = static_cast<uint8_t>(value);
        result.request.hasRgbTargetMin = true;
    }

    // Extract autoExposureEnabled (optional bool)
    if (root.containsKey("autoExposureEnabled") && root["autoExposureEnabled"].is<bool>()) {
        result.request.autoExposureEnabled = root["autoExposureEnabled"].as<bool>();
        result.request.hasAutoExposureEnabled = true;
    }

    // Extract autoExposureTarget (optional, 0-255)
    if (root.containsKey("autoExposureTarget") && root["autoExposureTarget"].is<int>()) {
        int value = root["autoExposureTarget"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "autoExposureTarget out of range (0-255): %d", value);
            return result;
        }
        result.request.autoExposureTarget = static_cast<uint8_t>(value);
        result.request.hasAutoExposureTarget = true;
    }

    // Extract gammaEnabled (optional bool)
    if (root.containsKey("gammaEnabled") && root["gammaEnabled"].is<bool>()) {
        result.request.gammaEnabled = root["gammaEnabled"].as<bool>();
        result.request.hasGammaEnabled = true;
    }

    // Extract gammaValue (optional float, 1.0f-3.0f)
    if (root.containsKey("gammaValue") && root["gammaValue"].is<float>()) {
        float val = root["gammaValue"].as<float>();
        if (val < 1.0f || val > 3.0f) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "gammaValue out of range (1.0-3.0): %.2f", val);
            return result;
        }
        result.request.gammaValue = val;
        result.request.hasGammaValue = true;
    }

    // Extract brownGuardrailEnabled (optional bool)
    if (root.containsKey("brownGuardrailEnabled") && root["brownGuardrailEnabled"].is<bool>()) {
        result.request.brownGuardrailEnabled = root["brownGuardrailEnabled"].as<bool>();
        result.request.hasBrownGuardrailEnabled = true;
    }

    // Extract maxGreenPercentOfRed (optional, 0-255)
    if (root.containsKey("maxGreenPercentOfRed") && root["maxGreenPercentOfRed"].is<int>()) {
        int value = root["maxGreenPercentOfRed"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "maxGreenPercentOfRed out of range (0-255): %d", value);
            return result;
        }
        result.request.maxGreenPercentOfRed = static_cast<uint8_t>(value);
        result.request.hasMaxGreenPercentOfRed = true;
    }

    // Extract maxBluePercentOfRed (optional, 0-255)
    if (root.containsKey("maxBluePercentOfRed") && root["maxBluePercentOfRed"].is<int>()) {
        int value = root["maxBluePercentOfRed"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "maxBluePercentOfRed out of range (0-255): %d", value);
            return result;
        }
        result.request.maxBluePercentOfRed = static_cast<uint8_t>(value);
        result.request.hasMaxBluePercentOfRed = true;
    }

    result.success = true;
    return result;
}

} // namespace codec
} // namespace lightwaveos
