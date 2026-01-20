/**
 * @file HttpParameterCodec.cpp
 * @brief HTTP parameter codec implementation
 *
 * Single canonical JSON parser for parameter HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpParameterCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

HttpParametersSetDecodeResult HttpParameterCodec::decodeSet(JsonObjectConst root) {
    HttpParametersSetDecodeResult result;
    result.request = HttpParametersSetRequest();

    auto decodeOptionalUint8 = [&root, &result](const char* key, bool& hasValue, uint8_t& outValue) -> bool {
        if (!root.containsKey(key)) {
            return true;
        }
        if (!root[key].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "Invalid field '%s'", key);
            return false;
        }
        int value = root[key].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "Field '%s' out of range (0-255)", key);
            return false;
        }
        hasValue = true;
        outValue = static_cast<uint8_t>(value);
        return true;
    };

    if (!decodeOptionalUint8("brightness", result.request.hasBrightness, result.request.brightness)) return result;
    if (!decodeOptionalUint8("speed", result.request.hasSpeed, result.request.speed)) return result;
    if (!decodeOptionalUint8("paletteId", result.request.hasPaletteId, result.request.paletteId)) return result;
    if (!decodeOptionalUint8("intensity", result.request.hasIntensity, result.request.intensity)) return result;
    if (!decodeOptionalUint8("saturation", result.request.hasSaturation, result.request.saturation)) return result;
    if (!decodeOptionalUint8("complexity", result.request.hasComplexity, result.request.complexity)) return result;
    if (!decodeOptionalUint8("variation", result.request.hasVariation, result.request.variation)) return result;
    if (!decodeOptionalUint8("hue", result.request.hasHue, result.request.hue)) return result;
    if (!decodeOptionalUint8("mood", result.request.hasMood, result.request.mood)) return result;
    if (!decodeOptionalUint8("fadeAmount", result.request.hasFadeAmount, result.request.fadeAmount)) return result;

    result.success = true;
    return result;
}

// ============================================================================
// Encode Functions
// ============================================================================

void HttpParameterCodec::encodeGet(const HttpParametersGetData& data, JsonObject& obj) {
    obj["brightness"] = data.brightness;
    obj["speed"] = data.speed;
    obj["paletteId"] = data.paletteId;
}

void HttpParameterCodec::encodeGetExtended(const HttpParametersGetExtendedData& data, JsonObject& obj) {
    obj["brightness"] = data.brightness;
    obj["speed"] = data.speed;
    obj["paletteId"] = data.paletteId;
    obj["hue"] = data.hue;
    obj["intensity"] = data.intensity;
    obj["saturation"] = data.saturation;
    obj["complexity"] = data.complexity;
    obj["variation"] = data.variation;
    obj["mood"] = data.mood;
    obj["fadeAmount"] = data.fadeAmount;
}

} // namespace codec
} // namespace lightwaveos
