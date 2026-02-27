// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsBatchCodec.cpp
 * @brief WebSocket batch codec implementation
 *
 * Single canonical JSON parser for batch WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsBatchCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

BatchExecuteDecodeResult WsBatchCodec::decodeExecute(JsonObjectConst root) {
    BatchExecuteDecodeResult result;
    result.request = BatchExecuteRequest();
    
    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;
    
    // Validate operations array exists and is an array (required)
    if (!root.containsKey("operations") || !root["operations"].is<JsonArrayConst>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'operations' (must be array)");
        return result;
    }
    result.request.hasOperations = true;
    
    result.success = true;
    return result;
}

// ============================================================================
// Encode Functions
// ============================================================================

void WsBatchCodec::encodeExecuteResult(const BatchExecuteResponseData& data, JsonObject& obj) {
    obj["processed"] = data.processed;
    obj["failed"] = data.failed;
}

} // namespace codec
} // namespace lightwaveos
