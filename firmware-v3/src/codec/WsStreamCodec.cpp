// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsStreamCodec.cpp
 * @brief WebSocket stream codec implementation
 *
 * Single canonical JSON parser for stream WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsStreamCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

StreamSimpleDecodeResult WsStreamCodec::decodeSimple(JsonObjectConst root) {
    StreamSimpleDecodeResult result;
    result.request = StreamSimpleRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void WsStreamCodec::encodeLedStreamSubscribed(uint32_t clientId, uint16_t frameSize, uint8_t frameVersion, uint8_t numStrips, uint16_t ledsPerStrip, uint8_t targetFps, uint8_t magicByte, JsonObject& data) {
    data["clientId"] = clientId;
    data["frameSize"] = frameSize;
    data["frameVersion"] = frameVersion;
    data["numStrips"] = numStrips;
    data["ledsPerStrip"] = ledsPerStrip;
    data["targetFps"] = targetFps;
    data["magicByte"] = magicByte;
    data["accepted"] = true;
}

void WsStreamCodec::encodeLedStreamUnsubscribed(uint32_t clientId, JsonObject& data) {
    data["clientId"] = clientId;
}

void WsStreamCodec::encodeValidationSubscribed(uint32_t clientId, size_t sampleSize, size_t maxSamplesPerFrame, uint8_t targetFps, JsonObject& data) {
    data["clientId"] = clientId;
    data["sampleSize"] = static_cast<uint32_t>(sampleSize);
    data["maxSamplesPerFrame"] = static_cast<uint32_t>(maxSamplesPerFrame);
    data["targetFps"] = targetFps;
    data["accepted"] = true;
}

void WsStreamCodec::encodeValidationUnsubscribed(uint32_t clientId, JsonObject& data) {
    data["clientId"] = clientId;
}

void WsStreamCodec::encodeBenchmarkSubscribed(uint32_t clientId, size_t frameSize, uint8_t targetFps, uint8_t magicByte, JsonObject& data) {
    data["clientId"] = clientId;
    data["frameSize"] = static_cast<uint32_t>(frameSize);
    data["targetFps"] = targetFps;
    data["magicByte"] = magicByte;
    data["accepted"] = true;
}

void WsStreamCodec::encodeBenchmarkUnsubscribed(uint32_t clientId, JsonObject& data) {
    data["clientId"] = clientId;
}

void WsStreamCodec::encodeBenchmarkStarted(JsonObject& data) {
    data["active"] = true;
}

void WsStreamCodec::encodeBenchmarkStopped(float avgTotalUs, float avgGoertzelUs, float cpuLoadPercent, uint32_t hopCount, uint16_t peakTotalUs, JsonObject& data) {
    data["active"] = false;
    JsonObject results = data["results"].to<JsonObject>();
    results["avgTotalUs"] = avgTotalUs;
    results["avgGoertzelUs"] = avgGoertzelUs;
    results["cpuLoadPercent"] = cpuLoadPercent;
    results["hopCount"] = hopCount;
    results["peakTotalUs"] = peakTotalUs;
}

void WsStreamCodec::encodeBenchmarkStats(bool streaming, float avgTotalUs, float avgGoertzelUs, float avgDcAgcUs, float avgChromaUs, uint16_t peakTotalUs, float cpuLoadPercent, uint32_t hopCount, JsonObject& data) {
    data["streaming"] = streaming;
    
    JsonObject timing = data["timing"].to<JsonObject>();
    timing["avgTotalUs"] = avgTotalUs;
    timing["avgGoertzelUs"] = avgGoertzelUs;
    timing["avgDcAgcUs"] = avgDcAgcUs;
    timing["avgChromaUs"] = avgChromaUs;
    timing["peakTotalUs"] = peakTotalUs;
    
    JsonObject load = data["load"].to<JsonObject>();
    load["cpuPercent"] = cpuLoadPercent;
    load["hopCount"] = hopCount;
}

void WsStreamCodec::encodeStreamRejected(const char* type, const char* requestId, const char* errorCode, const char* errorMessage, JsonDocument& response) {
    response["type"] = type;
    if (requestId != nullptr && strlen(requestId) > 0) {
        response["requestId"] = requestId;
    }
    response["success"] = false;
    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = errorCode;
    error["message"] = errorMessage;
}

} // namespace codec
} // namespace lightwaveos
