/**
 * @file WsPaletteCodec.cpp
 * @brief WebSocket palette codec implementation
 *
 * Single canonical JSON parser for palette WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsPaletteCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

PalettesListDecodeResult WsPaletteCodec::decodeList(JsonObjectConst root) {
    PalettesListDecodeResult result;
    result.request = PalettesListRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract page (optional, default: 1, min: 1)
    if (root["page"].is<int>()) {
        int page = root["page"].as<int>();
        if (page < 1) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "page must be >= 1: %d", page);
            return result;
        }
        result.request.page = static_cast<uint8_t>(page);
    }

    // Extract limit (optional, default: 20, range: 1-50)
    if (root["limit"].is<int>()) {
        int limit = root["limit"].as<int>();
        if (limit < 1 || limit > 50) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "limit out of range (1-50): %d", limit);
            return result;
        }
        result.request.limit = static_cast<uint8_t>(limit);
    }

    result.success = true;
    return result;
}

PalettesGetDecodeResult WsPaletteCodec::decodeGet(JsonObjectConst root) {
    PalettesGetDecodeResult result;
    result.request = PalettesGetRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract paletteId (required)
    if (!root["paletteId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'paletteId'");
        return result;
    }
    int paletteId = root["paletteId"].as<int>();
    if (paletteId < 0) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "paletteId must be >= 0: %d", paletteId);
        return result;
    }
    result.request.paletteId = static_cast<uint8_t>(paletteId);

    result.success = true;
    return result;
}

PalettesSetDecodeResult WsPaletteCodec::decodeSet(JsonObjectConst root) {
    PalettesSetDecodeResult result;
    result.request = PalettesSetRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract paletteId (required)
    if (!root["paletteId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'paletteId'");
        return result;
    }
    int paletteId = root["paletteId"].as<int>();
    if (paletteId < 0) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "paletteId must be >= 0: %d", paletteId);
        return result;
    }
    result.request.paletteId = static_cast<uint8_t>(paletteId);

    result.success = true;
    return result;
}

} // namespace codec
} // namespace lightwaveos
