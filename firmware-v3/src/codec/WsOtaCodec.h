/**
 * @file WsOtaCodec.h
 * @brief JSON codec for WebSocket OTA commands parsing and validation
 *
 * Single canonical location for parsing WebSocket OTA command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from OTA WS commands.
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
// OTA Check Request
// ============================================================================

struct OtaCheckRequest {
    const char* requestId;
    
    OtaCheckRequest() : requestId("") {}
};

struct OtaCheckDecodeResult {
    bool success;
    OtaCheckRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    OtaCheckDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// OTA Begin Request
// ============================================================================

struct OtaBeginRequest {
    uint32_t size;          // Firmware/filesystem image size in bytes
    const char* md5;        // Expected MD5 hash (optional)
    const char* token;      // OTA authentication token (required)
    const char* version;    // Incoming firmware version string (optional, e.g. "2.1.0")
    bool force;             // Force update even if version is older/same (default: true for backward compat)
    const char* target;     // "firmware" (default) or "filesystem" (optional)
    const char* requestId;

    OtaBeginRequest() : size(0), md5(nullptr), token(nullptr), version(nullptr), force(true), target("firmware"), requestId("") {}
};

struct OtaBeginDecodeResult {
    bool success;
    OtaBeginRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    OtaBeginDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// OTA Chunk Request
// ============================================================================

struct OtaChunkRequest {
    uint32_t offset;        // Byte offset in firmware
    const char* data;       // Base64-encoded chunk data
    const char* requestId;
    
    OtaChunkRequest() : offset(0), data(nullptr), requestId("") {}
};

struct OtaChunkDecodeResult {
    bool success;
    OtaChunkRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    OtaChunkDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// OTA Verify Request
// ============================================================================

struct OtaVerifyRequest {
    const char* md5;        // Optional MD5 hash for verification
    const char* requestId;
    
    OtaVerifyRequest() : md5(nullptr), requestId("") {}
};

struct OtaVerifyDecodeResult {
    bool success;
    OtaVerifyRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    OtaVerifyDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// WsOtaCodec - Decode Functions
// ============================================================================

class WsOtaCodec {
public:
    /**
     * @brief Decode ota.check request
     */
    static OtaCheckDecodeResult decodeOtaCheck(JsonObjectConst root);
    
    /**
     * @brief Decode ota.begin request
     */
    static OtaBeginDecodeResult decodeOtaBegin(JsonObjectConst root);
    
    /**
     * @brief Decode ota.chunk request
     */
    static OtaChunkDecodeResult decodeOtaChunk(JsonObjectConst root);
    
    /**
     * @brief Decode ota.abort request (no parameters)
     */
    static bool decodeOtaAbort(JsonObjectConst root, const char** requestId);
    
    /**
     * @brief Decode ota.verify request
     */
    static OtaVerifyDecodeResult decodeOtaVerify(JsonObjectConst root);
    
    // ========================================================================
    // Encode Functions (for responses)
    // ========================================================================
    
    /**
     * @brief Encode ota.status response
     */
    static void encodeOtaStatus(JsonObject& data, const char* version,
                                uint32_t versionNumber,
                                uint32_t sketchSize, uint32_t freeSpace,
                                bool otaAvailable);
    
    /**
     * @brief Encode ota.ready response
     */
    static void encodeOtaReady(JsonObject& data, uint32_t totalSize);
    
    /**
     * @brief Encode ota.progress response
     */
    static void encodeOtaProgress(JsonObject& data, uint32_t offset, 
                                 uint32_t total, uint8_t percent);
    
    /**
     * @brief Encode ota.complete response
     */
    static void encodeOtaComplete(JsonObject& data, bool rebooting);
    
    /**
     * @brief Encode ota.error response
     */
    static void encodeOtaError(JsonObject& data, const char* code, 
                              const char* message);
    
    /**
     * @brief Encode ota.aborted response
     */
    static void encodeOtaAborted(JsonObject& data);

private:
    /**
     * @brief Helper to check for unknown keys in JSON object
     */
    static bool hasUnknownKeys(JsonObjectConst root, const char* const* allowedKeys, size_t keyCount);
};

} // namespace codec
} // namespace lightwaveos
