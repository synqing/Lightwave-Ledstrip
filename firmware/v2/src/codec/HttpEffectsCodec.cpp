/**
 * @file HttpEffectsCodec.cpp
 * @brief HTTP effects codec implementation
 *
 * Single canonical JSON parser for effects HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpEffectsCodec.h"
#include "../config/limits.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

HttpEffectsSetDecodeResult HttpEffectsCodec::decodeSet(JsonObjectConst root) {
    HttpEffectsSetDecodeResult result;
    result.request = HttpEffectsSetRequest();
    
    // Extract effectId (required, 0 to MAX_EFFECTS-1)
    if (!root["effectId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'effectId'");
        return result;
    }
    int effectId = root["effectId"].as<int>();
    if (effectId < 0 || effectId >= limits::MAX_EFFECTS) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "effectId out of range (0-%d): %d", limits::MAX_EFFECTS - 1, effectId);
        return result;
    }
    result.request.effectId = static_cast<EffectId>(effectId);

    // Extract transition flag (optional, default: false)
    result.request.useTransition = root["transition"] | false;
    
    // Extract transitionType (optional, default: 0)
    if (root["transitionType"].is<int>()) {
        int transType = root["transitionType"].as<int>();
        if (transType < 0 || transType > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "transitionType out of range (0-255): %d", transType);
            return result;
        }
        result.request.transitionType = static_cast<uint8_t>(transType);
    } else {
        result.request.transitionType = 0;
    }
    
    result.success = true;
    return result;
}

HttpEffectsParametersSetDecodeResult HttpEffectsCodec::decodeParametersSet(JsonObjectConst root) {
    HttpEffectsParametersSetDecodeResult result;
    result.request = HttpEffectsParametersSetRequest();
    
    // Extract effectId (required, 0 to MAX_EFFECTS-1)
    if (!root["effectId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'effectId'");
        return result;
    }
    int effectId = root["effectId"].as<int>();
    if (effectId < 0 || effectId >= limits::MAX_EFFECTS) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "effectId out of range (0-%d): %d", limits::MAX_EFFECTS - 1, effectId);
        return result;
    }
    result.request.effectId = static_cast<EffectId>(effectId);

    // Extract parameters object (required)
    if (!root.containsKey("parameters") || !root["parameters"].is<JsonObjectConst>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'parameters' (must be object)");
        return result;
    }
    result.request.hasParameters = true;
    result.request.parameters = root["parameters"].as<JsonObjectConst>();
    
    result.success = true;
    return result;
}

// ============================================================================
// Encode Functions
// ============================================================================

void HttpEffectsCodec::encodeListPagination(const HttpEffectsListPaginationData& data, JsonObject& obj) {
    obj["total"] = data.total;
    obj["offset"] = data.offset;
    obj["limit"] = data.limit;
}

void HttpEffectsCodec::encodeListCompatPagination(const HttpEffectsListCompatPaginationData& data, JsonObject& obj) {
    obj["page"] = data.page;
    obj["limit"] = data.limit;
    obj["total"] = data.total;
    obj["pages"] = data.pages;
}

void HttpEffectsCodec::encodeList(const HttpEffectsListData& data, JsonObject& obj) {
    encodeListPagination(data.pagination, obj);
    JsonObject pagination = obj["pagination"].to<JsonObject>();
    encodeListCompatPagination(data.compatPagination, pagination);

    JsonArray effects = obj["effects"].to<JsonArray>();
    for (size_t i = 0; i < data.effectsCount; ++i) {
        const HttpEffectsListItemData& item = data.effects[i];
        JsonObject effect = effects.add<JsonObject>();
        effect["id"] = item.id;
        effect["name"] = item.name;
        effect["category"] = item.categoryName;
        effect["categoryId"] = item.categoryId;
        effect["isAudioReactive"] = item.isAudioReactive;
        effect["isIEffect"] = item.isIEffect;
        if (item.description) {
            effect["description"] = item.description;
        }
        if (item.hasVersion) {
            effect["version"] = item.version;
        }
        if (item.author) {
            effect["author"] = item.author;
        }
        if (item.ieffectCategory) {
            effect["ieffectCategory"] = item.ieffectCategory;
        }
        if (item.includeFeatures) {
            JsonObject features = effect["features"].to<JsonObject>();
            features["centerOrigin"] = item.features.centerOrigin;
            features["usesSpeed"] = item.features.usesSpeed;
            features["usesPalette"] = item.features.usesPalette;
            features["zoneAware"] = item.features.zoneAware;
        }
    }

    JsonArray categories = obj["categories"].to<JsonArray>();
    for (size_t i = 0; i < data.categoriesCount; ++i) {
        JsonObject cat = categories.add<JsonObject>();
        cat["id"] = data.categories[i].id;
        cat["name"] = data.categories[i].name;
    }

    obj["count"] = data.count;
}

void HttpEffectsCodec::encodeCurrent(const HttpEffectsCurrentData& data, JsonObject& obj) {
    obj["effectId"] = data.effectId;
    obj["name"] = data.name;
    obj["brightness"] = data.brightness;
    obj["speed"] = data.speed;
    obj["paletteId"] = data.paletteId;
    obj["hue"] = data.hue;
    obj["intensity"] = data.intensity;
    obj["saturation"] = data.saturation;
    obj["complexity"] = data.complexity;
    obj["variation"] = data.variation;
    obj["isIEffect"] = data.isIEffect;
    if (data.description) {
        obj["description"] = data.description;
    }
    if (data.hasVersion) {
        obj["version"] = data.version;
    }
}

void HttpEffectsCodec::encodeParametersGet(const HttpEffectsParametersGetData& data, JsonObject& obj) {
    obj["effectId"] = data.effectId;
    obj["name"] = data.name;
    obj["hasParameters"] = data.hasParameters;

    JsonArray params = obj["parameters"].to<JsonArray>();
    if (!data.hasParameters || !data.parameters) {
        return;
    }

    for (size_t i = 0; i < data.parameterCount; ++i) {
        const HttpEffectParameterItemData& param = data.parameters[i];
        JsonObject p = params.add<JsonObject>();
        p["name"] = param.name;
        p["displayName"] = param.displayName;
        p["min"] = param.minValue;
        p["max"] = param.maxValue;
        p["default"] = param.defaultValue;
        p["value"] = param.value;
    }
}

void HttpEffectsCodec::encodeParametersSetResult(const HttpEffectsParametersSetResultData& data, JsonObject& obj) {
    obj["effectId"] = data.effectId;
    obj["name"] = data.name;

    JsonArray queued = obj["queued"].to<JsonArray>();
    for (size_t i = 0; i < data.queuedCount; ++i) {
        queued.add(data.queued[i]);
    }

    JsonArray failed = obj["failed"].to<JsonArray>();
    for (size_t i = 0; i < data.failedCount; ++i) {
        failed.add(data.failed[i]);
    }
}

void HttpEffectsCodec::encodeMetadata(const HttpEffectsMetadataData& data, JsonObject& obj) {
    obj["id"] = data.id;
    obj["name"] = data.name;
    obj["isIEffect"] = data.isIEffect;
    if (data.description) {
        obj["description"] = data.description;
    }
    if (data.hasVersion) {
        obj["version"] = data.version;
    }
    if (data.author) {
        obj["author"] = data.author;
    }
    if (data.ieffectCategory) {
        obj["ieffectCategory"] = data.ieffectCategory;
    }

    obj["family"] = data.family ? data.family : "Unknown";
    obj["familyId"] = data.familyId;
    if (data.story) {
        obj["story"] = data.story;
    }
    if (data.opticalIntent) {
        obj["opticalIntent"] = data.opticalIntent;
    }

    JsonArray tags = obj["tags"].to<JsonArray>();
    if (data.tags.tags) {
        for (size_t i = 0; i < data.tags.count; ++i) {
            tags.add(data.tags.tags[i]);
        }
    }

    JsonObject properties = obj["properties"].to<JsonObject>();
    properties["centerOrigin"] = data.properties.centerOrigin;
    properties["symmetricStrips"] = data.properties.symmetricStrips;
    properties["paletteAware"] = data.properties.paletteAware;
    properties["speedResponsive"] = data.properties.speedResponsive;

    JsonObject recommended = obj["recommended"].to<JsonObject>();
    recommended["brightness"] = data.recommended.brightness;
    recommended["speed"] = data.recommended.speed;
}

void HttpEffectsCodec::encodeFamilies(const HttpEffectsFamiliesData& data, JsonObject& obj) {
    JsonArray families = obj["families"].to<JsonArray>();
    for (size_t i = 0; i < data.familyCount; ++i) {
        JsonObject familyObj = families.add<JsonObject>();
        familyObj["id"] = data.families[i].id;
        familyObj["name"] = data.families[i].name;
        familyObj["count"] = data.families[i].count;
    }
    obj["total"] = data.total;
}

} // namespace codec
} // namespace lightwaveos
