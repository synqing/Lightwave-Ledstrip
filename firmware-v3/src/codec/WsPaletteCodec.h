// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsPaletteCodec.h
 * @brief JSON codec for WebSocket palette commands parsing and validation
 *
 * Single canonical location for parsing WebSocket palette command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from palette WS commands.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

/**
 * @brief Decoded palettes.list request
 */
struct PalettesListRequest {
    uint8_t page;            // Optional (1+, default: 1)
    uint8_t limit;           // Optional (1-50, default: 20)
    const char* requestId;   // Optional
    
    PalettesListRequest() : page(1), limit(20), requestId("") {}
};

struct PalettesListDecodeResult {
    bool success;
    PalettesListRequest request;
    char errorMsg[MAX_ERROR_MSG];

    PalettesListDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded palettes.get request
 */
struct PalettesGetRequest {
    uint8_t paletteId;       // Required (0-N, validated externally)
    const char* requestId;   // Optional
    
    PalettesGetRequest() : paletteId(255), requestId("") {}
};

struct PalettesGetDecodeResult {
    bool success;
    PalettesGetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    PalettesGetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded palettes.set request
 */
struct PalettesSetRequest {
    uint8_t paletteId;       // Required (0-N, validated externally)
    const char* requestId;   // Optional
    
    PalettesSetRequest() : paletteId(255), requestId("") {}
};

struct PalettesSetDecodeResult {
    bool success;
    PalettesSetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    PalettesSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief WebSocket Palette Command JSON Codec
 *
 * Single canonical parser for palette WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsPaletteCodec {
public:
    static PalettesListDecodeResult decodeList(JsonObjectConst root);
    static PalettesGetDecodeResult decodeGet(JsonObjectConst root);
    static PalettesSetDecodeResult decodeSet(JsonObjectConst root);
};

} // namespace codec
} // namespace lightwaveos
