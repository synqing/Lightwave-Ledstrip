// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsNarrativeCodec.cpp
 * @brief WebSocket narrative codec implementation
 *
 * Single canonical JSON parser for narrative WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsNarrativeCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

NarrativeSimpleDecodeResult WsNarrativeCodec::decodeSimple(JsonObjectConst root) {
    NarrativeSimpleDecodeResult result;
    result.request = NarrativeSimpleRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    result.success = true;
    return result;
}

NarrativeConfigDecodeResult WsNarrativeCodec::decodeConfig(JsonObjectConst root) {
    NarrativeConfigDecodeResult result;
    result.request = NarrativeConfigRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Determine if this is a SET operation (has any config field)
    result.request.isSet = root.containsKey("durations") || root.containsKey("enabled") || root.containsKey("curves") ||
                           root.containsKey("holdBreathe") || root.containsKey("snapAmount") || root.containsKey("durationVariance");

    if (!result.request.isSet) {
        // GET operation - no fields to decode
        result.success = true;
        return result;
    }

    // SET operation - decode optional fields
    
    // Durations
    if (root.containsKey("durations") && root["durations"].is<JsonObjectConst>()) {
        JsonObjectConst durations = root["durations"].as<JsonObjectConst>();
        result.request.hasDurations = true;
        
        if (durations.containsKey("build") && durations["build"].is<float>()) {
            result.request.buildDuration = durations["build"].as<float>();
        } else {
            result.request.buildDuration = durations["build"] | 1.5f;
        }
        
        if (durations.containsKey("hold") && durations["hold"].is<float>()) {
            result.request.holdDuration = durations["hold"].as<float>();
        } else {
            result.request.holdDuration = durations["hold"] | 0.5f;
        }
        
        if (durations.containsKey("release") && durations["release"].is<float>()) {
            result.request.releaseDuration = durations["release"].as<float>();
        } else {
            result.request.releaseDuration = durations["release"] | 1.5f;
        }
        
        if (durations.containsKey("rest") && durations["rest"].is<float>()) {
            result.request.restDuration = durations["rest"].as<float>();
        } else {
            result.request.restDuration = durations["rest"] | 0.5f;
        }
    }

    // Curves
    if (root.containsKey("curves") && root["curves"].is<JsonObjectConst>()) {
        JsonObjectConst curves = root["curves"].as<JsonObjectConst>();
        result.request.hasCurves = true;
        
        if (curves.containsKey("build") && curves["build"].is<int>()) {
            int buildCurve = curves["build"].as<int>();
            if (buildCurve >= 0 && buildCurve <= 255) {
                result.request.buildCurveId = static_cast<uint8_t>(buildCurve);
            } else {
                result.request.buildCurveId = curves["build"] | 1U;
            }
        } else {
            result.request.buildCurveId = curves["build"] | 1U;
        }
        
        if (curves.containsKey("release") && curves["release"].is<int>()) {
            int releaseCurve = curves["release"].as<int>();
            if (releaseCurve >= 0 && releaseCurve <= 255) {
                result.request.releaseCurveId = static_cast<uint8_t>(releaseCurve);
            } else {
                result.request.releaseCurveId = curves["release"] | 6U;
            }
        } else {
            result.request.releaseCurveId = curves["release"] | 6U;
        }
    }

    // holdBreathe
    if (root.containsKey("holdBreathe")) {
        result.request.hasHoldBreathe = true;
        if (root["holdBreathe"].is<float>()) {
            result.request.holdBreathe = root["holdBreathe"].as<float>();
        } else {
            result.request.holdBreathe = root["holdBreathe"] | 0.0f;
        }
    }

    // snapAmount
    if (root.containsKey("snapAmount")) {
        result.request.hasSnapAmount = true;
        if (root["snapAmount"].is<float>()) {
            result.request.snapAmount = root["snapAmount"].as<float>();
        } else {
            result.request.snapAmount = root["snapAmount"] | 0.0f;
        }
    }

    // durationVariance
    if (root.containsKey("durationVariance")) {
        result.request.hasDurationVariance = true;
        if (root["durationVariance"].is<float>()) {
            result.request.durationVariance = root["durationVariance"].as<float>();
        } else {
            result.request.durationVariance = root["durationVariance"] | 0.0f;
        }
    }

    // enabled
    if (root.containsKey("enabled")) {
        result.request.hasEnabled = true;
        if (root["enabled"].is<bool>()) {
            result.request.enabled = root["enabled"].as<bool>();
        } else {
            result.request.enabled = root["enabled"] | false;
        }
    }

    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void WsNarrativeCodec::encodeStatus(bool enabled, float tensionPercent, float phaseT, float cycleT, const char* phaseName, uint8_t phaseId, float buildDuration, float holdDuration, float releaseDuration, float restDuration, float totalDuration, float tempoMultiplier, float complexityScaling, JsonObject& data) {
    data["enabled"] = enabled;
    data["tension"] = tensionPercent;
    data["phaseT"] = phaseT;
    data["cycleT"] = cycleT;
    data["phase"] = phaseName ? phaseName : "UNKNOWN";
    data["phaseId"] = phaseId;
    
    JsonObject durations = data["durations"].to<JsonObject>();
    durations["build"] = buildDuration;
    durations["hold"] = holdDuration;
    durations["release"] = releaseDuration;
    durations["rest"] = restDuration;
    durations["total"] = totalDuration;
    
    data["tempoMultiplier"] = tempoMultiplier;
    data["complexityScaling"] = complexityScaling;
}

void WsNarrativeCodec::encodeConfigGet(float buildDuration, float holdDuration, float releaseDuration, float restDuration, float totalDuration, uint8_t buildCurveId, uint8_t releaseCurveId, float holdBreathe, float snapAmount, float durationVariance, bool enabled, JsonObject& data) {
    JsonObject durations = data["durations"].to<JsonObject>();
    durations["build"] = buildDuration;
    durations["hold"] = holdDuration;
    durations["release"] = releaseDuration;
    durations["rest"] = restDuration;
    durations["total"] = totalDuration;
    
    JsonObject curves = data["curves"].to<JsonObject>();
    curves["build"] = buildCurveId;
    curves["release"] = releaseCurveId;
    
    data["holdBreathe"] = holdBreathe;
    data["snapAmount"] = snapAmount;
    data["durationVariance"] = durationVariance;
    data["enabled"] = enabled;
}

void WsNarrativeCodec::encodeConfigSetResult(bool updated, JsonObject& data) {
    data["message"] = updated ? "Narrative config updated" : "No changes";
    data["updated"] = updated;
}

} // namespace codec
} // namespace lightwaveos
