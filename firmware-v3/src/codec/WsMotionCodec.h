/**
 * @file WsMotionCodec.h
 * @brief JSON codec for WebSocket motion commands parsing and validation
 *
 * Single canonical location for parsing WebSocket motion command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from motion WS commands.
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

/**
 * @brief Maximum particles in momentum engine
 */
static constexpr uint8_t MAX_PARTICLES = 32;

// ============================================================================
// Codec-Owned Enums (to avoid including MotionEngine headers)
// ============================================================================

/**
 * @brief Boundary mode for particles (matches BoundaryMode in MotionEngine)
 */
enum class MotionBoundary : uint8_t {
    WRAP = 0,
    BOUNCE = 1,
    CLAMP = 2,
    DIE = 3
};

/**
 * @brief Speed modulation type (matches SpeedModulator::ModulationType)
 */
enum class MotionModType : uint8_t {
    CONSTANT = 0,
    SINE_WAVE = 1,
    EXPONENTIAL_DECAY = 2
};

// ============================================================================
// Simple Request (requestId only)
// ============================================================================

/**
 * @brief Decoded simple request (requestId only)
 */
struct MotionSimpleRequest {
    const char* requestId;   // Optional
    
    MotionSimpleRequest() : requestId("") {}
};

struct MotionSimpleDecodeResult {
    bool success;
    MotionSimpleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionSimpleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Phase Commands
// ============================================================================

/**
 * @brief Decoded motion.phase.setOffset request
 */
struct MotionPhaseSetOffsetRequest {
    float degreesValue;      // Required (0-360) - named to avoid Arduino.h 'degrees' macro
    const char* requestId;   // Optional
    
    MotionPhaseSetOffsetRequest() : degreesValue(-1.0f), requestId("") {}
};

struct MotionPhaseSetOffsetDecodeResult {
    bool success;
    MotionPhaseSetOffsetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionPhaseSetOffsetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
        request = MotionPhaseSetOffsetRequest();
    }
};

/**
 * @brief Decoded motion.phase.enableAutoRotate request
 */
struct MotionPhaseEnableAutoRotateRequest {
    float degreesPerSecond;  // Required (>= 0)
    const char* requestId;   // Optional
    
    MotionPhaseEnableAutoRotateRequest() : degreesPerSecond(0.0f), requestId("") {}
};

struct MotionPhaseEnableAutoRotateDecodeResult {
    bool success;
    MotionPhaseEnableAutoRotateRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionPhaseEnableAutoRotateDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Speed Commands
// ============================================================================

/**
 * @brief Decoded motion.speed.setModulation request
 */
struct MotionSpeedSetModulationRequest {
    MotionModType modType;   // Required (CONSTANT, SINE_WAVE, EXPONENTIAL_DECAY)
    float depth;             // Required (0.0-1.0)
    const char* requestId;    // Optional
    
    MotionSpeedSetModulationRequest() : modType(MotionModType::CONSTANT), depth(0.5f), requestId("") {}
};

struct MotionSpeedSetModulationDecodeResult {
    bool success;
    MotionSpeedSetModulationRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionSpeedSetModulationDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded motion.speed.setBaseSpeed request
 */
struct MotionSpeedSetBaseSpeedRequest {
    float speed;             // Required (>= 0)
    const char* requestId;   // Optional
    
    MotionSpeedSetBaseSpeedRequest() : speed(-1.0f), requestId("") {}
};

struct MotionSpeedSetBaseSpeedDecodeResult {
    bool success;
    MotionSpeedSetBaseSpeedRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionSpeedSetBaseSpeedDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Momentum Commands
// ============================================================================

/**
 * @brief Decoded motion.momentum.addParticle request
 */
struct MotionMomentumAddParticleRequest {
    float position;          // Required (0.0-1.0, but codec doesn't enforce range)
    float velocity;           // Required
    float mass;               // Required
    MotionBoundary boundary;  // Required (WRAP, BOUNCE, CLAMP, DIE)
    const char* requestId;   // Optional
    
    MotionMomentumAddParticleRequest() : position(0.5f), velocity(0.0f), mass(1.0f), boundary(MotionBoundary::WRAP), requestId("") {}
};

struct MotionMomentumAddParticleDecodeResult {
    bool success;
    MotionMomentumAddParticleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionMomentumAddParticleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded motion.momentum.applyForce request
 */
struct MotionMomentumApplyForceRequest {
    int particleId;          // Required (0-MAX_PARTICLES-1)
    float force;             // Required
    const char* requestId;   // Optional
    
    MotionMomentumApplyForceRequest() : particleId(-1), force(0.0f), requestId("") {}
};

struct MotionMomentumApplyForceDecodeResult {
    bool success;
    MotionMomentumApplyForceRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionMomentumApplyForceDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded motion.momentum.getParticle request
 */
struct MotionMomentumGetParticleRequest {
    int particleId;          // Required (0-MAX_PARTICLES-1)
    const char* requestId;   // Optional
    
    MotionMomentumGetParticleRequest() : particleId(-1), requestId("") {}
};

struct MotionMomentumGetParticleDecodeResult {
    bool success;
    MotionMomentumGetParticleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionMomentumGetParticleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded motion.momentum.update request
 */
struct MotionMomentumUpdateRequest {
    float deltaTime;         // Optional (default 0.016f)
    const char* requestId;   // Optional
    
    MotionMomentumUpdateRequest() : deltaTime(0.016f), requestId("") {}
};

struct MotionMomentumUpdateDecodeResult {
    bool success;
    MotionMomentumUpdateRequest request;
    char errorMsg[MAX_ERROR_MSG];

    MotionMomentumUpdateDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief WebSocket Motion Command JSON Codec
 *
 * Single canonical parser for motion WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsMotionCodec {
public:
    // Decode functions (request parsing)
    static MotionSimpleDecodeResult decodeSimple(JsonObjectConst root);
    static MotionPhaseSetOffsetDecodeResult decodePhaseSetOffset(JsonObjectConst root);
    static MotionPhaseEnableAutoRotateDecodeResult decodePhaseEnableAutoRotate(JsonObjectConst root);
    static MotionSpeedSetModulationDecodeResult decodeSpeedSetModulation(JsonObjectConst root);
    static MotionSpeedSetBaseSpeedDecodeResult decodeSpeedSetBaseSpeed(JsonObjectConst root);
    static MotionMomentumAddParticleDecodeResult decodeMomentumAddParticle(JsonObjectConst root);
    static MotionMomentumApplyForceDecodeResult decodeMomentumApplyForce(JsonObjectConst root);
    static MotionMomentumGetParticleDecodeResult decodeMomentumGetParticle(JsonObjectConst root);
    static MotionMomentumUpdateDecodeResult decodeMomentumUpdate(JsonObjectConst root);
    
    // Encoder functions (response encoding)
    // Populate JsonObject data from domain objects
    
    // Status and control encoders
    static void encodeGetStatus(bool enabled, float phaseOffset, float autoRotateSpeed, float baseSpeed, JsonObject& data);
    static void encodeEnabled(bool enabled, JsonObject& data);
    
    // Phase encoders
    static void encodePhaseSetOffset(float degrees, JsonObject& data);
    static void encodePhaseEnableAutoRotate(float degreesPerSecond, bool autoRotate, JsonObject& data);
    static void encodePhaseGetPhase(float degrees, float radians, JsonObject& data);
    
    // Speed encoders
    static void encodeSpeedSetModulation(const char* typeStr, float depth, JsonObject& data);
    static void encodeSpeedSetBaseSpeed(float speed, JsonObject& data);
    
    // Momentum encoders
    static void encodeMomentumGetStatus(uint8_t activeCount, uint8_t maxParticles, JsonObject& data);
    static void encodeMomentumAddParticle(int particleId, bool success, JsonObject& data);
    static void encodeMomentumApplyForce(int particleId, float force, bool applied, JsonObject& data);
    static void encodeMomentumGetParticle(int particleId, float position, float velocity, float mass, bool alive, JsonObject& data);
    static void encodeMomentumReset(const char* message, uint8_t activeCount, JsonObject& data);
    static void encodeMomentumUpdate(float deltaTime, uint8_t activeCount, bool updated, JsonObject& data);
};

} // namespace codec
} // namespace lightwaveos
