/**
 * @file HttpBatchCodec.cpp
 * @brief HTTP batch codec implementation
 *
 * Single canonical JSON parser for batch HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpBatchCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Encode Functions
// ============================================================================

void HttpBatchCodec::encodeExecute(const HttpBatchExecuteData& data, JsonObject& obj) {
    obj["processed"] = data.processed;
    obj["failed"] = data.failed;
}

} // namespace codec
} // namespace lightwaveos
