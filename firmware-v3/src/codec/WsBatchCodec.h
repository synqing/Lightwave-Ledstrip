/**
 * @file WsBatchCodec.h
 * @brief JSON codec for WebSocket batch commands parsing and validation
 *
 * Single canonical location for parsing WebSocket batch command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from batch WS commands.
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
#include "WsCommonCodec.h"

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Decode Request Structs
// ============================================================================

/**
 * @brief Decoded batch.execute request
 * Note: We only validate that operations exists and is an array.
 * The handler will access the mutable array from the original document.
 */
struct BatchExecuteRequest {
    const char* requestId;           // Optional
    bool hasOperations;              // True if operations array present
    
    BatchExecuteRequest() : requestId(""), hasOperations(false) {}
};

struct BatchExecuteDecodeResult {
    bool success;
    BatchExecuteRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    BatchExecuteDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Batch execute response data
 */
struct BatchExecuteResponseData {
    uint8_t processed;
    uint8_t failed;
    
    BatchExecuteResponseData() : processed(0), failed(0) {}
};

/**
 * @brief WebSocket Batch Command JSON Codec
 *
 * Single canonical parser for batch WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsBatchCodec {
public:
    // Decode functions (request parsing)
    static BatchExecuteDecodeResult decodeExecute(JsonObjectConst root);
    
    // Encode functions (response building)
    static void encodeExecuteResult(const BatchExecuteResponseData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
