// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsMotionCommands.cpp
 * @brief WebSocket motion command handlers implementation
 */

#include "WsMotionCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../effects/enhancement/MotionEngine.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::enhancement;

static void handleMotionGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& engine = MotionEngine::getInstance();
    
    String response = buildWsResponse("motion.getStatus", requestId, [&engine](JsonObject& data) {
        data["enabled"] = engine.isEnabled();
        data["phaseOffset"] = engine.getPhaseController().stripPhaseOffset;
        data["autoRotateSpeed"] = engine.getPhaseController().phaseVelocity;
        data["baseSpeed"] = engine.getSpeedModulator().getBaseSpeed();
    });
    client->text(response);
}

static void handleMotionEnable(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& engine = MotionEngine::getInstance();
    engine.enable();
    
    String response = buildWsResponse("motion.enable", requestId, [](JsonObject& data) {
        data["enabled"] = true;
    });
    client->text(response);
}

static void handleMotionDisable(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& engine = MotionEngine::getInstance();
    engine.disable();
    
    String response = buildWsResponse("motion.disable", requestId, [](JsonObject& data) {
        data["enabled"] = false;
    });
    client->text(response);
}

static void handleMotionPhaseSetOffset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    float degrees = doc["degrees"] | -1.0f;
    
    if (degrees < 0.0f || degrees > 360.0f) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "degrees must be 0-360", requestId));
        return;
    }
    
    auto& engine = MotionEngine::getInstance();
    engine.getPhaseController().setStripPhaseOffset(degrees);
    
    String response = buildWsResponse("motion.phase.setOffset", requestId, [degrees](JsonObject& data) {
        data["degrees"] = degrees;
    });
    client->text(response);
}

static void handleMotionPhaseEnableAutoRotate(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    float degreesPerSecond = doc["degreesPerSecond"] | 0.0f;
    
    auto& engine = MotionEngine::getInstance();
    engine.getPhaseController().enableAutoRotate(degreesPerSecond);
    
    String response = buildWsResponse("motion.phase.enableAutoRotate", requestId, [degreesPerSecond](JsonObject& data) {
        data["degreesPerSecond"] = degreesPerSecond;
        data["autoRotate"] = true;
    });
    client->text(response);
}

static void handleMotionPhaseGetPhase(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& engine = MotionEngine::getInstance();
    float radians = engine.getPhaseController().getStripPhaseRadians();
    float degrees = radians * 180.0f / 3.14159265f;
    
    String response = buildWsResponse("motion.phase.getPhase", requestId, [degrees, radians](JsonObject& data) {
        data["degrees"] = degrees;
        data["radians"] = radians;
    });
    client->text(response);
}

static void handleMotionSpeedSetModulation(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    const char* modTypeStr = doc["type"] | "";
    float depth = doc["depth"] | 0.5f;
    
    if (depth < 0.0f || depth > 1.0f) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "depth must be 0.0-1.0", requestId));
        return;
    }
    
    SpeedModulator::ModulationType modType;
    if (strcmp(modTypeStr, "CONSTANT") == 0) {
        modType = SpeedModulator::MOD_CONSTANT;
    } else if (strcmp(modTypeStr, "SINE_WAVE") == 0) {
        modType = SpeedModulator::MOD_SINE_WAVE;
    } else if (strcmp(modTypeStr, "EXPONENTIAL_DECAY") == 0) {
        modType = SpeedModulator::MOD_EXPONENTIAL_DECAY;
    } else {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Invalid type (CONSTANT, SINE_WAVE, EXPONENTIAL_DECAY)", requestId));
        return;
    }
    
    auto& engine = MotionEngine::getInstance();
    engine.getSpeedModulator().setModulation(modType, depth);
    
    String response = buildWsResponse("motion.speed.setModulation", requestId, [modTypeStr, depth](JsonObject& data) {
        data["type"] = modTypeStr;
        data["depth"] = depth;
    });
    client->text(response);
}

static void handleMotionSpeedSetBaseSpeed(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    float speed = doc["speed"] | -1.0f;
    
    if (speed < 0.0f) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "speed required", requestId));
        return;
    }
    
    auto& engine = MotionEngine::getInstance();
    engine.getSpeedModulator().setBaseSpeed(speed);
    
    String response = buildWsResponse("motion.speed.setBaseSpeed", requestId, [speed](JsonObject& data) {
        data["speed"] = speed;
    });
    client->text(response);
}

static void handleMotionMomentumGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& momentum = MotionEngine::getInstance().getMomentumEngine();
    
    String response = buildWsResponse("motion.momentum.getStatus", requestId, [&momentum](JsonObject& data) {
        data["activeCount"] = momentum.getActiveCount();
        data["maxParticles"] = MomentumEngine::MAX_PARTICLES;
    });
    client->text(response);
}

static void handleMotionMomentumAddParticle(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& momentum = MotionEngine::getInstance().getMomentumEngine();
    
    float pos = doc["position"] | 0.5f;
    float vel = doc["velocity"] | 0.0f;
    float mass = doc["mass"] | 1.0f;
    
    BoundaryMode mode = BOUNDARY_WRAP;
    String boundaryStr = doc["boundary"] | "WRAP";
    if (boundaryStr == "BOUNCE") mode = BOUNDARY_BOUNCE;
    else if (boundaryStr == "CLAMP") mode = BOUNDARY_CLAMP;
    else if (boundaryStr == "DIE") mode = BOUNDARY_DIE;
    
    int id = momentum.addParticle(pos, vel, mass, CRGB::White, mode);
    
    String response = buildWsResponse("motion.momentum.addParticle", requestId, [id](JsonObject& data) {
        data["particleId"] = id;
        data["success"] = (id >= 0);
    });
    client->text(response);
}

static void handleMotionMomentumApplyForce(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("particleId")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "particleId required", requestId));
        return;
    }
    
    int particleId = doc["particleId"] | -1;
    float force = doc["force"] | 0.0f;
    
    if (particleId < 0 || particleId >= MomentumEngine::MAX_PARTICLES) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "particleId out of range (0-31)", requestId));
        return;
    }
    
    auto& momentum = MotionEngine::getInstance().getMomentumEngine();
    momentum.applyForce(particleId, force);
    
    String response = buildWsResponse("motion.momentum.applyForce", requestId, [particleId, force](JsonObject& data) {
        data["particleId"] = particleId;
        data["force"] = force;
        data["applied"] = true;
    });
    client->text(response);
}

static void handleMotionMomentumGetParticle(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("particleId")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "particleId required", requestId));
        return;
    }
    
    int particleId = doc["particleId"] | -1;
    
    if (particleId < 0 || particleId >= MomentumEngine::MAX_PARTICLES) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "particleId out of range (0-31)", requestId));
        return;
    }
    
    auto& momentum = MotionEngine::getInstance().getMomentumEngine();
    auto* particle = momentum.getParticle(particleId);
    
    if (!particle) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to get particle", requestId));
        return;
    }
    
    String response = buildWsResponse("motion.momentum.getParticle", requestId, [particleId, particle](JsonObject& data) {
        data["particleId"] = particleId;
        data["position"] = particle->position;
        data["velocity"] = particle->velocity;
        data["mass"] = particle->mass;
        data["alive"] = particle->active;
    });
    client->text(response);
}

static void handleMotionMomentumReset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& momentum = MotionEngine::getInstance().getMomentumEngine();
    
    momentum.reset();
    
    String response = buildWsResponse("motion.momentum.reset", requestId, [](JsonObject& data) {
        data["message"] = "All particles cleared";
        data["activeCount"] = 0;
    });
    client->text(response);
}

static void handleMotionMomentumUpdate(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    float deltaTime = doc["deltaTime"] | 0.016f;
    
    auto& momentum = MotionEngine::getInstance().getMomentumEngine();
    momentum.update(deltaTime);
    
    String response = buildWsResponse("motion.momentum.update", requestId, [deltaTime, &momentum](JsonObject& data) {
        data["deltaTime"] = deltaTime;
        data["activeCount"] = momentum.getActiveCount();
        data["updated"] = true;
    });
    client->text(response);
}

void registerWsMotionCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("motion.getStatus", handleMotionGetStatus);
    WsCommandRouter::registerCommand("motion.enable", handleMotionEnable);
    WsCommandRouter::registerCommand("motion.disable", handleMotionDisable);
    WsCommandRouter::registerCommand("motion.phase.setOffset", handleMotionPhaseSetOffset);
    WsCommandRouter::registerCommand("motion.phase.enableAutoRotate", handleMotionPhaseEnableAutoRotate);
    WsCommandRouter::registerCommand("motion.phase.getPhase", handleMotionPhaseGetPhase);
    WsCommandRouter::registerCommand("motion.speed.setModulation", handleMotionSpeedSetModulation);
    WsCommandRouter::registerCommand("motion.speed.setBaseSpeed", handleMotionSpeedSetBaseSpeed);
    WsCommandRouter::registerCommand("motion.momentum.getStatus", handleMotionMomentumGetStatus);
    WsCommandRouter::registerCommand("motion.momentum.addParticle", handleMotionMomentumAddParticle);
    WsCommandRouter::registerCommand("motion.momentum.applyForce", handleMotionMomentumApplyForce);
    WsCommandRouter::registerCommand("motion.momentum.getParticle", handleMotionMomentumGetParticle);
    WsCommandRouter::registerCommand("motion.momentum.reset", handleMotionMomentumReset);
    WsCommandRouter::registerCommand("motion.momentum.update", handleMotionMomentumUpdate);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

