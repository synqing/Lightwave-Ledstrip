/**
 * @file HttpEffectsCodec.h
 * @brief JSON codec for HTTP effects endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP effects endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from effects HTTP endpoints.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>
#include <cstring>
#include "WsEffectsCodec.h"  // Reuse POD structs

namespace lightwaveos {
namespace codec {

// Reuse MAX_ERROR_MSG from WsEffectsCodec (same namespace)

// ============================================================================
// Decode Request Structs (for POST/PUT endpoints)
// ============================================================================

/**
 * @brief Decoded effects.set request (HTTP version, simpler than WS)
 */
struct HttpEffectsSetRequest {
    EffectId effectId;               // Effect ID (stable namespaced)
    bool useTransition;               // Optional (default: false)
    uint8_t transitionType;           // Optional (default: 0)

    HttpEffectsSetRequest()
        : effectId(INVALID_EFFECT_ID), useTransition(false), transitionType(0) {}
};

struct HttpEffectsSetDecodeResult {
    bool success;
    HttpEffectsSetRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpEffectsSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded effects.parameters.set request (HTTP version)
 */
struct HttpEffectsParametersSetRequest {
    EffectId effectId;                // Effect ID (stable namespaced)
    bool hasParameters;               // True if parameters object present
    JsonObjectConst parameters;       // Dynamic parameters object (if hasParameters)

    HttpEffectsParametersSetRequest()
        : effectId(INVALID_EFFECT_ID), hasParameters(false), parameters() {}
};

struct HttpEffectsParametersSetDecodeResult {
    bool success;
    HttpEffectsParametersSetRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpEffectsParametersSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Pagination data for effects.list response
 */
struct HttpEffectsListPaginationData {
    int total;
    int offset;
    int limit;
    
    HttpEffectsListPaginationData() : total(0), offset(0), limit(20) {}
};

/**
 * @brief Effects list pagination (compat object)
 */
struct HttpEffectsListCompatPaginationData {
    int page;
    int limit;
    int total;
    int pages;
    
    HttpEffectsListCompatPaginationData() : page(1), limit(20), total(0), pages(1) {}
};

/**
 * @brief Effects list item feature flags
 */
struct HttpEffectsListFeatureData {
    bool centerOrigin;
    bool usesSpeed;
    bool usesPalette;
    bool zoneAware;
    
    HttpEffectsListFeatureData()
        : centerOrigin(true), usesSpeed(true), usesPalette(true), zoneAware(true) {}
};

/**
 * @brief Effects list item data
 */
struct HttpEffectsListItemData {
    uint8_t id;
    const char* name;
    const char* categoryName;
    int categoryId;
    bool isAudioReactive;
    bool isIEffect;
    const char* description;
    uint8_t version;
    bool hasVersion;
    const char* author;
    const char* ieffectCategory;
    bool includeFeatures;
    HttpEffectsListFeatureData features;
    
    HttpEffectsListItemData()
        : id(0), name(""), categoryName(""), categoryId(0), isAudioReactive(false),
          isIEffect(false), description(nullptr), version(0), hasVersion(false),
          author(nullptr), ieffectCategory(nullptr), includeFeatures(false), features() {}
};

/**
 * @brief Effects category data
 */
struct HttpEffectsCategoryData {
    int id;
    const char* name;
    
    HttpEffectsCategoryData() : id(0), name("") {}
};

/**
 * @brief Effects list response data
 */
struct HttpEffectsListData {
    HttpEffectsListPaginationData pagination;
    HttpEffectsListCompatPaginationData compatPagination;
    const HttpEffectsListItemData* effects;
    size_t effectsCount;
    const HttpEffectsCategoryData* categories;
    size_t categoriesCount;
    size_t count;
    
    HttpEffectsListData()
        : effects(nullptr), effectsCount(0), categories(nullptr), categoriesCount(0), count(0) {}
};

/**
 * @brief Current effect response data
 */
struct HttpEffectsCurrentData {
    EffectId effectId;
    const char* name;
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteId;
    uint8_t hue;
    uint8_t intensity;
    uint8_t saturation;
    uint8_t complexity;
    uint8_t variation;
    bool isIEffect;
    const char* description;
    uint8_t version;
    bool hasVersion;
    
    HttpEffectsCurrentData()
        : effectId(0), name(""), brightness(0), speed(0), paletteId(0), hue(0),
          intensity(0), saturation(0), complexity(0), variation(0),
          isIEffect(false), description(nullptr), version(0), hasVersion(false) {}
};

/**
 * @brief Effect parameter item data
 */
struct HttpEffectParameterItemData {
    const char* name;
    const char* displayName;
    float minValue;
    float maxValue;
    float defaultValue;
    float value;
    const char* type;
    float step;
    const char* group;
    const char* unit;
    bool advanced;
    
    HttpEffectParameterItemData()
        : name(""), displayName(""), minValue(0.0f), maxValue(0.0f),
          defaultValue(0.0f), value(0.0f), type("float"), step(0.01f),
          group(""), unit(""), advanced(false) {}
};

/**
 * @brief Effect parameters response data
 */
struct HttpEffectsParametersGetData {
    EffectId effectId;
    const char* name;
    bool hasParameters;
    const HttpEffectParameterItemData* parameters;
    size_t parameterCount;
    const char* persistenceMode;
    bool persistenceDirty;
    const char* persistenceLastError;
    
    HttpEffectsParametersGetData()
        : effectId(0), name(""), hasParameters(false), parameters(nullptr), parameterCount(0),
          persistenceMode("volatile"), persistenceDirty(false), persistenceLastError(nullptr) {}
};

/**
 * @brief Effect parameters set result data
 */
struct HttpEffectsParametersSetResultData {
    EffectId effectId;
    const char* name;
    const char** queued;
    size_t queuedCount;
    const char** failed;
    size_t failedCount;
    
    HttpEffectsParametersSetResultData()
        : effectId(0), name(""), queued(nullptr), queuedCount(0), failed(nullptr), failedCount(0) {}
};

/**
 * @brief Effect metadata tags data
 */
struct HttpEffectsTagsData {
    const char** tags;
    size_t count;
    
    HttpEffectsTagsData() : tags(nullptr), count(0) {}
};

/**
 * @brief Effect metadata properties data
 */
struct HttpEffectsPropertiesData {
    bool centerOrigin;
    bool symmetricStrips;
    bool paletteAware;
    bool speedResponsive;
    
    HttpEffectsPropertiesData()
        : centerOrigin(true), symmetricStrips(true), paletteAware(true), speedResponsive(true) {}
};

/**
 * @brief Effect metadata recommended data
 */
struct HttpEffectsRecommendedData {
    uint8_t brightness;
    uint8_t speed;
    
    HttpEffectsRecommendedData() : brightness(0), speed(0) {}
};

/**
 * @brief Effect metadata response data
 */
struct HttpEffectsMetadataData {
    uint8_t id;
    const char* name;
    bool isIEffect;
    const char* description;
    uint8_t version;
    bool hasVersion;
    const char* author;
    const char* ieffectCategory;
    const char* family;
    uint8_t familyId;
    const char* story;
    const char* opticalIntent;
    HttpEffectsTagsData tags;
    HttpEffectsPropertiesData properties;
    HttpEffectsRecommendedData recommended;
    
    HttpEffectsMetadataData()
        : id(0), name(""), isIEffect(false), description(nullptr), version(0),
          hasVersion(false), author(nullptr), ieffectCategory(nullptr), family(nullptr), familyId(0),
          story(nullptr), opticalIntent(nullptr), tags(), properties(), recommended() {}
};

/**
 * @brief Effect family data
 */
struct HttpEffectsFamilyItemData {
    uint8_t id;
    const char* name;
    uint8_t count;
    
    HttpEffectsFamilyItemData() : id(0), name(""), count(0) {}
};

/**
 * @brief Effect families response data
 */
struct HttpEffectsFamiliesData {
    const HttpEffectsFamilyItemData* families;
    size_t familyCount;
    size_t total;
    
    HttpEffectsFamiliesData() : families(nullptr), familyCount(0), total(0) {}
};

/**
 * @brief HTTP Effects Command JSON Codec
 *
 * Single canonical parser for effects HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpEffectsCodec {
public:
    // Decode functions (request parsing)
    static HttpEffectsSetDecodeResult decodeSet(JsonObjectConst root);
    static HttpEffectsParametersSetDecodeResult decodeParametersSet(JsonObjectConst root);
    
    // Encode functions (response building)
    static void encodeListPagination(const HttpEffectsListPaginationData& data, JsonObject& obj);
    static void encodeListCompatPagination(const HttpEffectsListCompatPaginationData& data, JsonObject& obj);
    static void encodeList(const HttpEffectsListData& data, JsonObject& obj);
    static void encodeCurrent(const HttpEffectsCurrentData& data, JsonObject& obj);
    static void encodeParametersGet(const HttpEffectsParametersGetData& data, JsonObject& obj);
    static void encodeParametersSetResult(const HttpEffectsParametersSetResultData& data, JsonObject& obj);
    static void encodeMetadata(const HttpEffectsMetadataData& data, JsonObject& obj);
    static void encodeFamilies(const HttpEffectsFamiliesData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
