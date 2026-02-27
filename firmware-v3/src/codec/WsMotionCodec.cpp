// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsMotionCodec.cpp
 * @brief WebSocket motion codec implementation
 *
 * Single canonical JSON parser for motion WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsMotionCodec.h"
#include "WsCommonCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Simple Request
// ============================================================================

MotionSimpleDecodeResult WsMotionCodec::decodeSimple(JsonObjectConst root) {
    MotionSimpleDecodeResult result;
    result.request = MotionSimpleRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    result.success = true;
    return result;
}

// ============================================================================
// Phase Commands
// ============================================================================

MotionPhaseSetOffsetDecodeResult WsMotionCodec::decodePhaseSetOffset(JsonObjectConst root) {
    MotionPhaseSetOffsetDecodeResult result;
    result.request = MotionPhaseSetOffsetRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract degrees (required, 0-360)
    if (!root.containsKey("degrees")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'degrees'");
        return result;
    }
    if (!root["degrees"].is<float>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'degrees' must be a float");
        return result;
    }
    float degrees = root["degrees"].as<float>();
    if (degrees < 0.0f || degrees > 360.0f) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "degrees out of range (0-360): %.2f", degrees);
        return result;
    }
    result.request.degreesValue = degrees;

    result.success = true;
    return result;
}

MotionPhaseEnableAutoRotateDecodeResult WsMotionCodec::decodePhaseEnableAutoRotate(JsonObjectConst root) {
    MotionPhaseEnableAutoRotateDecodeResult result;
    result.request = MotionPhaseEnableAutoRotateRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract degreesPerSecond (required, >= 0)
    if (!root.containsKey("degreesPerSecond")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'degreesPerSecond'");
        return result;
    }
    if (!root["degreesPerSecond"].is<float>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'degreesPerSecond' must be a float");
        return result;
    }
    float degreesPerSecond = root["degreesPerSecond"].as<float>();
    if (degreesPerSecond < 0.0f) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "degreesPerSecond must be >= 0: %.2f", degreesPerSecond);
        return result;
    }
    result.request.degreesPerSecond = degreesPerSecond;

    result.success = true;
    return result;
}

// ============================================================================
// Speed Commands
// ============================================================================

MotionSpeedSetModulationDecodeResult WsMotionCodec::decodeSpeedSetModulation(JsonObjectConst root) {
    MotionSpeedSetModulationDecodeResult result;
    result.request = MotionSpeedSetModulationRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract type (required string)
    if (!root.containsKey("type")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'type'");
        return result;
    }
    if (!root["type"].is<const char*>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'type' must be a string");
        return result;
    }
    const char* typeStr = root["type"].as<const char*>();
    if (strcmp(typeStr, "CONSTANT") == 0) {
        result.request.modType = MotionModType::CONSTANT;
    } else if (strcmp(typeStr, "SINE_WAVE") == 0) {
        result.request.modType = MotionModType::SINE_WAVE;
    } else if (strcmp(typeStr, "EXPONENTIAL_DECAY") == 0) {
        result.request.modType = MotionModType::EXPONENTIAL_DECAY;
    } else {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Invalid type '%s' (must be CONSTANT, SINE_WAVE, or EXPONENTIAL_DECAY)", typeStr);
        return result;
    }

    // Extract depth (required, 0.0-1.0)
    if (!root.containsKey("depth")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'depth'");
        return result;
    }
    if (!root["depth"].is<float>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'depth' must be a float");
        return result;
    }
    float depth = root["depth"].as<float>();
    if (depth < 0.0f || depth > 1.0f) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "depth out of range (0.0-1.0): %.2f", depth);
        return result;
    }
    result.request.depth = depth;

    result.success = true;
    return result;
}

MotionSpeedSetBaseSpeedDecodeResult WsMotionCodec::decodeSpeedSetBaseSpeed(JsonObjectConst root) {
    MotionSpeedSetBaseSpeedDecodeResult result;
    result.request = MotionSpeedSetBaseSpeedRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract speed (required, >= 0)
    if (!root.containsKey("speed")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'speed'");
        return result;
    }
    if (!root["speed"].is<float>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'speed' must be a float");
        return result;
    }
    float speed = root["speed"].as<float>();
    if (speed < 0.0f) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "speed must be >= 0: %.2f", speed);
        return result;
    }
    result.request.speed = speed;

    result.success = true;
    return result;
}

// ============================================================================
// Momentum Commands
// ============================================================================

MotionMomentumAddParticleDecodeResult WsMotionCodec::decodeMomentumAddParticle(JsonObjectConst root) {
    MotionMomentumAddParticleDecodeResult result;
    result.request = MotionMomentumAddParticleRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract position (optional, default 0.5f)
    if (root.containsKey("position") && root["position"].is<float>()) {
        result.request.position = root["position"].as<float>();
    }

    // Extract velocity (optional, default 0.0f)
    if (root.containsKey("velocity") && root["velocity"].is<float>()) {
        result.request.velocity = root["velocity"].as<float>();
    }

    // Extract mass (optional, default 1.0f)
    if (root.containsKey("mass") && root["mass"].is<float>()) {
        result.request.mass = root["mass"].as<float>();
    }

    // Extract boundary (optional string, default WRAP)
    if (root.containsKey("boundary") && root["boundary"].is<const char*>()) {
        const char* boundaryStr = root["boundary"].as<const char*>();
        if (strcmp(boundaryStr, "BOUNCE") == 0) {
            result.request.boundary = MotionBoundary::BOUNCE;
        } else if (strcmp(boundaryStr, "CLAMP") == 0) {
            result.request.boundary = MotionBoundary::CLAMP;
        } else if (strcmp(boundaryStr, "DIE") == 0) {
            result.request.boundary = MotionBoundary::DIE;
        } else {
            // Default to WRAP if not recognized
            result.request.boundary = MotionBoundary::WRAP;
        }
    }

    result.success = true;
    return result;
}

MotionMomentumApplyForceDecodeResult WsMotionCodec::decodeMomentumApplyForce(JsonObjectConst root) {
    MotionMomentumApplyForceDecodeResult result;
    result.request = MotionMomentumApplyForceRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract particleId (required, 0-MAX_PARTICLES-1)
    if (!root.containsKey("particleId")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'particleId'");
        return result;
    }
    if (!root["particleId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'particleId' must be an integer");
        return result;
    }
    int particleId = root["particleId"].as<int>();
    if (particleId < 0 || particleId >= MAX_PARTICLES) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "particleId out of range (0-%d): %d", MAX_PARTICLES - 1, particleId);
        return result;
    }
    result.request.particleId = particleId;

    // Extract force (optional, default 0.0f)
    if (root.containsKey("force") && root["force"].is<float>()) {
        result.request.force = root["force"].as<float>();
    }

    result.success = true;
    return result;
}

MotionMomentumGetParticleDecodeResult WsMotionCodec::decodeMomentumGetParticle(JsonObjectConst root) {
    MotionMomentumGetParticleDecodeResult result;
    result.request = MotionMomentumGetParticleRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract particleId (required, 0-MAX_PARTICLES-1)
    if (!root.containsKey("particleId")) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'particleId'");
        return result;
    }
    if (!root["particleId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'particleId' must be an integer");
        return result;
    }
    int particleId = root["particleId"].as<int>();
    if (particleId < 0 || particleId >= MAX_PARTICLES) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "particleId out of range (0-%d): %d", MAX_PARTICLES - 1, particleId);
        return result;
    }
    result.request.particleId = particleId;

    result.success = true;
    return result;
}

MotionMomentumUpdateDecodeResult WsMotionCodec::decodeMomentumUpdate(JsonObjectConst root) {
    MotionMomentumUpdateDecodeResult result;
    result.request = MotionMomentumUpdateRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract deltaTime (optional, default 0.016f)
    if (root.containsKey("deltaTime") && root["deltaTime"].is<float>()) {
        result.request.deltaTime = root["deltaTime"].as<float>();
    }

    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void WsMotionCodec::encodeGetStatus(bool enabled, float phaseOffset, float autoRotateSpeed, float baseSpeed, JsonObject& data) {
    data["enabled"] = enabled;
    data["phaseOffset"] = phaseOffset;
    data["autoRotateSpeed"] = autoRotateSpeed;
    data["baseSpeed"] = baseSpeed;
}

void WsMotionCodec::encodeEnabled(bool enabled, JsonObject& data) {
    data["enabled"] = enabled;
}

void WsMotionCodec::encodePhaseSetOffset(float degrees, JsonObject& data) {
    data["degrees"] = degrees;
}

void WsMotionCodec::encodePhaseEnableAutoRotate(float degreesPerSecond, bool autoRotate, JsonObject& data) {
    data["degreesPerSecond"] = degreesPerSecond;
    data["autoRotate"] = autoRotate;
}

void WsMotionCodec::encodePhaseGetPhase(float degrees, float radians, JsonObject& data) {
    data["degrees"] = degrees;
    data["radians"] = radians;
}

void WsMotionCodec::encodeSpeedSetModulation(const char* typeStr, float depth, JsonObject& data) {
    data["type"] = typeStr ? typeStr : "";
    data["depth"] = depth;
}

void WsMotionCodec::encodeSpeedSetBaseSpeed(float speed, JsonObject& data) {
    data["speed"] = speed;
}

void WsMotionCodec::encodeMomentumGetStatus(uint8_t activeCount, uint8_t maxParticles, JsonObject& data) {
    data["activeCount"] = activeCount;
    data["maxParticles"] = maxParticles;
}

void WsMotionCodec::encodeMomentumAddParticle(int particleId, bool success, JsonObject& data) {
    data["particleId"] = particleId;
    data["success"] = success;
}

void WsMotionCodec::encodeMomentumApplyForce(int particleId, float force, bool applied, JsonObject& data) {
    data["particleId"] = particleId;
    data["force"] = force;
    data["applied"] = applied;
}

void WsMotionCodec::encodeMomentumGetParticle(int particleId, float position, float velocity, float mass, bool alive, JsonObject& data) {
    data["particleId"] = particleId;
    data["position"] = position;
    data["velocity"] = velocity;
    data["mass"] = mass;
    data["alive"] = alive;
}

void WsMotionCodec::encodeMomentumReset(const char* message, uint8_t activeCount, JsonObject& data) {
    data["message"] = message ? message : "";
    data["activeCount"] = activeCount;
}

void WsMotionCodec::encodeMomentumUpdate(float deltaTime, uint8_t activeCount, bool updated, JsonObject& data) {
    data["deltaTime"] = deltaTime;
    data["activeCount"] = activeCount;
    data["updated"] = updated;
}

} // namespace codec
} // namespace lightwaveos
