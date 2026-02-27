/**
 * @file HttpParameterCodec.h
 * @brief JSON codec for HTTP parameter endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP parameter endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from parameter HTTP endpoints.
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

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Parameters GET response data
 */
struct HttpParametersGetData {
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteId;
    
    HttpParametersGetData() : brightness(128), speed(15), paletteId(0) {}
};

/**
 * @brief Extended parameters GET response data
 */
struct HttpParametersGetExtendedData {
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteId;
    uint8_t hue;
    uint8_t intensity;
    uint8_t saturation;
    uint8_t complexity;
    uint8_t variation;
    uint8_t mood;
    uint8_t fadeAmount;
    
    HttpParametersGetExtendedData()
        : brightness(128), speed(15), paletteId(0), hue(0), intensity(0),
          saturation(0), complexity(0), variation(0), mood(0), fadeAmount(0) {}
};

/**
 * @brief Parameters set request data (optional fields)
 */
struct HttpParametersSetRequest {
    bool hasBrightness;
    uint8_t brightness;
    bool hasSpeed;
    uint8_t speed;
    bool hasPaletteId;
    uint8_t paletteId;
    bool hasIntensity;
    uint8_t intensity;
    bool hasSaturation;
    uint8_t saturation;
    bool hasComplexity;
    uint8_t complexity;
    bool hasVariation;
    uint8_t variation;
    bool hasHue;
    uint8_t hue;
    bool hasMood;
    uint8_t mood;
    bool hasFadeAmount;
    uint8_t fadeAmount;
    
    HttpParametersSetRequest()
        : hasBrightness(false), brightness(0),
          hasSpeed(false), speed(0),
          hasPaletteId(false), paletteId(0),
          hasIntensity(false), intensity(0),
          hasSaturation(false), saturation(0),
          hasComplexity(false), complexity(0),
          hasVariation(false), variation(0),
          hasHue(false), hue(0),
          hasMood(false), mood(0),
          hasFadeAmount(false), fadeAmount(0) {}
};

struct HttpParametersSetDecodeResult {
    bool success;
    HttpParametersSetRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpParametersSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief HTTP Parameter Command JSON Codec
 *
 * Single canonical parser for parameter HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpParameterCodec {
public:
    // Decode functions (request parsing)
    static HttpParametersSetDecodeResult decodeSet(JsonObjectConst root);

    // Encode functions (response building)
    static void encodeGet(const HttpParametersGetData& data, JsonObject& obj);
    static void encodeGetExtended(const HttpParametersGetExtendedData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
