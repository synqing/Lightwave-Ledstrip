// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpPaletteCodec.h
 * @brief JSON codec for HTTP palette endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP palette endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from palette HTTP endpoints.
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
#include "WsPaletteCodec.h"  // Reuse POD structs

namespace lightwaveos {
namespace codec {

// Reuse MAX_ERROR_MSG from WsPaletteCodec (same namespace)

// Reuse POD structs from WsPaletteCodec where applicable:
// - PalettesSetRequest (HTTP version is simpler, no requestId)

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Pagination data for palettes.list response
 */
struct HttpPalettesListPaginationData {
    int total;
    int offset;
    int limit;
    
    HttpPalettesListPaginationData() : total(0), offset(0), limit(20) {}
};

/**
 * @brief Pagination object for palettes list (compat)
 */
struct HttpPalettesListCompatPaginationData {
    int page;
    int limit;
    int total;
    int pages;
    
    HttpPalettesListCompatPaginationData() : page(1), limit(20), total(0), pages(1) {}
};

/**
 * @brief Palette category counts
 */
struct HttpPaletteCategoryCounts {
    int artistic;
    int scientific;
    int lgpOptimized;
    
    HttpPaletteCategoryCounts() : artistic(0), scientific(0), lgpOptimized(0) {}
};

/**
 * @brief Palette flags data
 */
struct HttpPaletteFlagsData {
    bool warm;
    bool cool;
    bool calm;
    bool vivid;
    bool cvdFriendly;
    bool whiteHeavy;
    
    HttpPaletteFlagsData()
        : warm(false), cool(false), calm(false), vivid(false), cvdFriendly(false), whiteHeavy(false) {}
};

/**
 * @brief Palette item data
 */
struct HttpPaletteItemData {
    uint8_t paletteId;
    const char* name;
    const char* category;
    HttpPaletteFlagsData flags;
    uint8_t avgBrightness;
    uint8_t maxBrightness;
    
    HttpPaletteItemData()
        : paletteId(0), name(""), category(""), flags(), avgBrightness(0), maxBrightness(0) {}
};

/**
 * @brief Palettes list response data
 */
struct HttpPalettesListData {
    HttpPalettesListPaginationData pagination;
    HttpPalettesListCompatPaginationData compatPagination;
    HttpPaletteCategoryCounts categories;
    const HttpPaletteItemData* palettes;
    size_t paletteCount;
    size_t count;
    
    HttpPalettesListData() : palettes(nullptr), paletteCount(0), count(0) {}
};

/**
 * @brief Decoded palettes.set request
 */
struct HttpPaletteSetRequest {
    uint8_t paletteId;
    
    HttpPaletteSetRequest() : paletteId(255) {}
};

struct HttpPaletteSetDecodeResult {
    bool success;
    HttpPaletteSetRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpPaletteSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief HTTP Palette Command JSON Codec
 *
 * Single canonical parser for palette HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpPaletteCodec {
public:
    // Decode functions (request parsing)
    static HttpPaletteSetDecodeResult decodeSet(JsonObjectConst root);

    // Encode functions (response building)
    static void encodeListPagination(const HttpPalettesListPaginationData& data, JsonObject& obj);
    static void encodeListCompatPagination(const HttpPalettesListCompatPaginationData& data, JsonObject& obj);
    static void encodeCategories(const HttpPaletteCategoryCounts& data, JsonObject& obj);
    static void encodePaletteItem(const HttpPaletteItemData& data, JsonObject& obj);
    static void encodeList(const HttpPalettesListData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
