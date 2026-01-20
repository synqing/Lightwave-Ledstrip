/**
 * @file HttpBatchCodec.h
 * @brief JSON codec for HTTP batch endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP batch endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from batch HTTP endpoints.
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
 * @brief Batch execute response data
 */
struct HttpBatchExecuteData {
    uint8_t processed;
    uint8_t failed;
    
    HttpBatchExecuteData() : processed(0), failed(0) {}
};

/**
 * @brief HTTP Batch Command JSON Codec
 *
 * Single canonical parser for batch HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpBatchCodec {
public:
    // Encode functions (response building)
    static void encodeExecute(const HttpBatchExecuteData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
