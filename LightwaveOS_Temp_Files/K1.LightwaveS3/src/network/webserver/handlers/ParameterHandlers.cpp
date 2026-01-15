/**
 * @file ParameterHandlers.cpp
 * @brief Parameter handlers implementation
 */

#include "ParameterHandlers.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../WebServer.h"  // For CachedRendererState
#include "../../../core/actors/NodeOrchestrator.h"
#include "../../../core/actors/RendererNode.h"

using namespace lightwaveos::nodes;
using namespace lightwaveos::network;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void ParameterHandlers::handleGet(AsyncWebServerRequest* request,
                                    const WebServer::CachedRendererState& cachedState) {
    sendSuccessResponse(request, [&cachedState](JsonObject& data) {
        // SAFE: Uses cached state (no cross-core access)
        data["brightness"] = cachedState.brightness;
        data["speed"] = cachedState.speed;
        data["paletteId"] = cachedState.paletteIndex;
        data["hue"] = cachedState.hue;
        data["intensity"] = cachedState.intensity;
        data["saturation"] = cachedState.saturation;
        data["complexity"] = cachedState.complexity;
        data["variation"] = cachedState.variation;
        data["mood"] = cachedState.mood;  // Sensory Bridge mood (0-255)
        data["fadeAmount"] = cachedState.fadeAmount;
    });
}

void ParameterHandlers::handleSet(AsyncWebServerRequest* request,
                                     uint8_t* data, size_t len,
                                     NodeOrchestrator& orchestrator,
                                     std::function<void()> broadcastStatus) {
    StaticJsonDocument<512> doc;
    // Parse and validate - all fields are optional but must be valid if present
    auto vr = RequestValidator::parseAndValidate(data, len, doc, RequestSchemas::SetParameters);
    if (!vr.valid) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          vr.errorCode, vr.errorMessage, vr.fieldName);
        return;
    }

    bool updated = false;

    if (doc.containsKey("brightness")) {
        uint8_t val = doc["brightness"];
        orchestrator.setBrightness(val);
        updated = true;
    }

    if (doc.containsKey("speed")) {
        uint8_t val = doc["speed"];
        // Range already validated by schema (1-100)
        orchestrator.setSpeed(val);
        updated = true;
    }

    if (doc.containsKey("paletteId")) {
        uint8_t val = doc["paletteId"];
        orchestrator.setPalette(val);
        updated = true;
    }

    if (doc.containsKey("intensity")) {
        uint8_t val = doc["intensity"];
        orchestrator.setIntensity(val);
        updated = true;
    }

    if (doc.containsKey("saturation")) {
        uint8_t val = doc["saturation"];
        orchestrator.setSaturation(val);
        updated = true;
    }

    if (doc.containsKey("complexity")) {
        uint8_t val = doc["complexity"];
        orchestrator.setComplexity(val);
        updated = true;
    }

    if (doc.containsKey("variation")) {
        uint8_t val = doc["variation"];
        orchestrator.setVariation(val);
        updated = true;
    }

    if (doc.containsKey("hue")) {
        uint8_t val = doc["hue"];
        orchestrator.setHue(val);
        updated = true;
    }

    if (doc.containsKey("mood")) {
        uint8_t val = doc["mood"];
        orchestrator.setMood(val);  // Sensory Bridge: 0=reactive, 255=smooth
        updated = true;
    }

    if (doc.containsKey("fadeAmount")) {
        uint8_t val = doc["fadeAmount"];
        orchestrator.setFadeAmount(val);
        updated = true;
    }

    if (updated) {
        sendSuccessResponse(request);
        broadcastStatus();
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "No valid parameters provided");
    }
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

