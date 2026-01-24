/**
 * @file WsStreamCodec.h
 * @brief JSON codec for WebSocket stream subscription commands parsing and validation
 *
 * Single canonical location for parsing WebSocket stream command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from stream WS commands.
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
 * @brief Decoded simple request (requestId only, for all stream commands)
 */
struct StreamSimpleRequest {
    const char* requestId;   // Optional
    
    StreamSimpleRequest() : requestId("") {}
};

struct StreamSimpleDecodeResult {
    bool success;
    StreamSimpleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    StreamSimpleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief WebSocket Stream Command JSON Codec
 *
 * Single canonical parser for stream WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsStreamCodec {
public:
    // Decode functions (request parsing)
    static StreamSimpleDecodeResult decodeSimple(JsonObjectConst root);
    
    // Encoder functions (response encoding)
    // Populate JsonObject data from domain objects
    
    // LED Stream encoders
    static void encodeLedStreamSubscribed(uint32_t clientId, uint16_t frameSize, uint8_t frameVersion, uint8_t numStrips, uint16_t ledsPerStrip, uint8_t targetFps, uint8_t magicByte, JsonObject& data);
    static void encodeLedStreamUnsubscribed(uint32_t clientId, JsonObject& data);
    
    // Validation Stream encoders
    static void encodeValidationSubscribed(uint32_t clientId, size_t sampleSize, size_t maxSamplesPerFrame, uint8_t targetFps, JsonObject& data);
    static void encodeValidationUnsubscribed(uint32_t clientId, JsonObject& data);
    
    // Benchmark Stream encoders
    static void encodeBenchmarkSubscribed(uint32_t clientId, size_t frameSize, uint8_t targetFps, uint8_t magicByte, JsonObject& data);
    static void encodeBenchmarkUnsubscribed(uint32_t clientId, JsonObject& data);
    static void encodeBenchmarkStarted(JsonObject& data);
    static void encodeBenchmarkStopped(float avgTotalUs, float avgGoertzelUs, float cpuLoadPercent, uint32_t hopCount, uint16_t peakTotalUs, JsonObject& data);
    static void encodeBenchmarkStats(bool streaming, float avgTotalUs, float avgGoertzelUs, float avgDcAgcUs, float avgChromaUs, uint16_t peakTotalUs, float cpuLoadPercent, uint32_t hopCount, JsonObject& data);
    
    // Error/rejection response encoders
    static void encodeStreamRejected(const char* type, const char* requestId, const char* errorCode, const char* errorMessage, JsonDocument& response);
};

} // namespace codec
} // namespace lightwaveos
