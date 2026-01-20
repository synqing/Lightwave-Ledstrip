/**
 * @file HttpPaletteCodec.cpp
 * @brief HTTP palette codec implementation
 *
 * Single canonical JSON parser for palette HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpPaletteCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

HttpPaletteSetDecodeResult HttpPaletteCodec::decodeSet(JsonObjectConst root) {
    HttpPaletteSetDecodeResult result;
    result.request = HttpPaletteSetRequest();

    if (!root["paletteId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'paletteId'");
        return result;
    }
    int paletteId = root["paletteId"].as<int>();
    if (paletteId < 0 || paletteId > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "paletteId out of range (0-255): %d", paletteId);
        return result;
    }
    result.request.paletteId = static_cast<uint8_t>(paletteId);
    result.success = true;
    return result;
}

// ============================================================================
// Encode Functions
// ============================================================================

void HttpPaletteCodec::encodeListPagination(const HttpPalettesListPaginationData& data, JsonObject& obj) {
    obj["total"] = data.total;
    obj["offset"] = data.offset;
    obj["limit"] = data.limit;
}

void HttpPaletteCodec::encodeListCompatPagination(const HttpPalettesListCompatPaginationData& data, JsonObject& obj) {
    obj["page"] = data.page;
    obj["limit"] = data.limit;
    obj["total"] = data.total;
    obj["pages"] = data.pages;
}

void HttpPaletteCodec::encodeCategories(const HttpPaletteCategoryCounts& data, JsonObject& obj) {
    obj["artistic"] = data.artistic;
    obj["scientific"] = data.scientific;
    obj["lgpOptimized"] = data.lgpOptimized;
}

void HttpPaletteCodec::encodePaletteItem(const HttpPaletteItemData& data, JsonObject& obj) {
    obj["paletteId"] = data.paletteId;
    obj["name"] = data.name;
    obj["category"] = data.category;

    JsonObject flags = obj["flags"].to<JsonObject>();
    flags["warm"] = data.flags.warm;
    flags["cool"] = data.flags.cool;
    flags["calm"] = data.flags.calm;
    flags["vivid"] = data.flags.vivid;
    flags["cvdFriendly"] = data.flags.cvdFriendly;
    flags["whiteHeavy"] = data.flags.whiteHeavy;

    obj["avgBrightness"] = data.avgBrightness;
    obj["maxBrightness"] = data.maxBrightness;
}

void HttpPaletteCodec::encodeList(const HttpPalettesListData& data, JsonObject& obj) {
    encodeListPagination(data.pagination, obj);
    JsonObject pagination = obj["pagination"].to<JsonObject>();
    encodeListCompatPagination(data.compatPagination, pagination);

    JsonObject categories = obj["categories"].to<JsonObject>();
    encodeCategories(data.categories, categories);

    JsonArray palettes = obj["palettes"].to<JsonArray>();
    for (size_t i = 0; i < data.paletteCount; ++i) {
        JsonObject palette = palettes.add<JsonObject>();
        encodePaletteItem(data.palettes[i], palette);
    }

    obj["count"] = data.count;
}

} // namespace codec
} // namespace lightwaveos
