/**
 * @file HttpResponseCodec.cpp
 * @brief HTTP response codec implementation
 *
 * Single canonical JSON encoder for standard HTTP response fields.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpResponseCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Encode Functions
// ============================================================================

void HttpResponseCodec::encodeEnvelope(const HttpResponseEnvelopeData& data, JsonObject& obj) {
    obj["success"] = data.success;
    obj["timestamp"] = data.timestamp;
    obj["version"] = data.version;
}

} // namespace codec
} // namespace lightwaveos
