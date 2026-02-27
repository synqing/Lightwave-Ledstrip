/**
 * @file HttpNarrativeCodec.cpp
 * @brief HTTP narrative codec implementation
 *
 * Single canonical JSON parser for narrative HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpNarrativeCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

NarrativeConfigDecodeResult HttpNarrativeCodec::decodeConfigSet(JsonObjectConst root) {
    // Reuse WS codec decode function (HTTP just ignores requestId)
    return WsNarrativeCodec::decodeConfig(root);
}

void HttpNarrativeCodec::encodeStatus(bool enabled, float tensionPercent, float phaseT, float cycleT, const char* phaseName, uint8_t phaseId, float buildDuration, float holdDuration, float releaseDuration, float restDuration, float totalDuration, float tempoMultiplier, float complexityScaling, JsonObject& data) {
    WsNarrativeCodec::encodeStatus(enabled, tensionPercent, phaseT, cycleT, phaseName, phaseId, buildDuration, holdDuration, releaseDuration, restDuration, totalDuration, tempoMultiplier, complexityScaling, data);
}

void HttpNarrativeCodec::encodeConfigGet(float buildDuration, float holdDuration, float releaseDuration, float restDuration, float totalDuration, uint8_t buildCurveId, uint8_t releaseCurveId, float holdBreathe, float snapAmount, float durationVariance, bool enabled, JsonObject& data) {
    WsNarrativeCodec::encodeConfigGet(buildDuration, holdDuration, releaseDuration, restDuration, totalDuration, buildCurveId, releaseCurveId, holdBreathe, snapAmount, durationVariance, enabled, data);
}

void HttpNarrativeCodec::encodeConfigSetResult(bool updated, JsonObject& data) {
    WsNarrativeCodec::encodeConfigSetResult(updated, data);
}

} // namespace codec
} // namespace lightwaveos
