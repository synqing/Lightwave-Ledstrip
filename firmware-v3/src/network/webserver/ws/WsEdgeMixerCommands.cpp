/**
 * @file WsEdgeMixerCommands.cpp
 * @brief WebSocket edge mixer command handlers implementation
 */

#include "WsEdgeMixerCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../effects/enhancement/EdgeMixer.h"
#include "../../../core/actors/ActorSystem.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::enhancement;

/**
 * @brief Handle edge_mixer.set command
 *
 * Accepts any combination of: mode (0-6), spread (0-60), strength (0-255),
 * spatial (0-1), temporal (0-1).
 * Only provided fields are applied; omitted fields remain unchanged.
 */
static void handleEdgeMixerSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    // Validate-all-then-apply: extract and validate all fields before applying any
    uint8_t mode = 0; bool hasMode = false;
    uint8_t spread = 0; bool hasSpread = false;
    uint8_t strength = 0; bool hasStrength = false;
    uint8_t spatial = 0; bool hasSpatial = false;
    uint8_t temporal = 0; bool hasTemporal = false;

    if (doc.containsKey("mode")) {
        mode = doc["mode"] | 0;
        if (mode > 6) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "mode must be 0-6", requestId));
            return;
        }
        hasMode = true;
    }

    if (doc.containsKey("spread")) {
        spread = doc["spread"] | 30;
        if (spread > 60) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "spread must be 0-60", requestId));
            return;
        }
        hasSpread = true;
    }

    if (doc.containsKey("strength")) {
        strength = doc["strength"] | 255;
        hasStrength = true;
    }

    if (doc.containsKey("spatial")) {
        spatial = doc["spatial"] | 0;
        if (spatial > 1) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "spatial must be 0-1", requestId));
            return;
        }
        hasSpatial = true;
    }

    if (doc.containsKey("temporal")) {
        temporal = doc["temporal"] | 0;
        if (temporal > 1) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "temporal must be 0-1", requestId));
            return;
        }
        hasTemporal = true;
    }

    if (!hasMode && !hasSpread && !hasStrength && !hasSpatial && !hasTemporal) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Provide mode, spread, strength, spatial, or temporal", requestId));
        return;
    }

    // Snapshot current state; override with requested values for response.
    // Actor messages are queued (async), so the singleton may not reflect
    // the new state by the time we build the response.
    auto& mixer = EdgeMixer::getInstance();
    uint8_t rMode     = hasMode     ? mode     : static_cast<uint8_t>(mixer.getMode());
    uint8_t rSpread   = hasSpread   ? spread   : mixer.getSpread();
    uint8_t rStrength = hasStrength ? strength : mixer.getStrength();
    uint8_t rSpatial  = hasSpatial  ? spatial  : static_cast<uint8_t>(mixer.getSpatial());
    uint8_t rTemporal = hasTemporal ? temporal : static_cast<uint8_t>(mixer.getTemporal());

    // Apply all after all validated — route through ActorSystem for thread safety
    if (hasMode) ctx.actorSystem.setEdgeMixerMode(mode);
    if (hasSpread) ctx.actorSystem.setEdgeMixerSpread(spread);
    if (hasStrength) ctx.actorSystem.setEdgeMixerStrength(strength);
    if (hasSpatial) ctx.actorSystem.setEdgeMixerSpatial(spatial);
    if (hasTemporal) ctx.actorSystem.setEdgeMixerTemporal(temporal);

    // Respond with requested values (not stale singleton).
    String response = buildWsResponse("edge_mixer.set", requestId,
        [rMode, rSpread, rStrength, rSpatial, rTemporal](JsonObject& data) {
        data["mode"] = rMode;
        data["modeName"] = EdgeMixer::modeName(static_cast<EdgeMixerMode>(rMode));
        data["spread"] = rSpread;
        data["strength"] = rStrength;
        data["spatial"] = rSpatial;
        data["spatialName"] = EdgeMixer::spatialName(static_cast<EdgeMixerSpatial>(rSpatial));
        data["temporal"] = rTemporal;
        data["temporalName"] = EdgeMixer::temporalName(static_cast<EdgeMixerTemporal>(rTemporal));
    });
    client->text(response);
}

/**
 * @brief Handle edge_mixer.get command
 *
 * Returns current mode, spread, strength, spatial, and temporal.
 */
static void handleEdgeMixerGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& mixer = EdgeMixer::getInstance();

    String response = buildWsResponse("edge_mixer.get", requestId, [&mixer](JsonObject& data) {
        data["mode"] = static_cast<uint8_t>(mixer.getMode());
        data["modeName"] = mixer.modeName();
        data["spread"] = mixer.getSpread();
        data["strength"] = mixer.getStrength();
        data["spatial"] = static_cast<uint8_t>(mixer.getSpatial());
        data["spatialName"] = mixer.spatialName();
        data["temporal"] = static_cast<uint8_t>(mixer.getTemporal());
        data["temporalName"] = mixer.temporalName();
    });
    client->text(response);
}

/**
 * @brief Handle edge_mixer.save command
 *
 * Persists current settings to NVS.
 */
static void handleEdgeMixerSave(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    ctx.actorSystem.saveEdgeMixerToNVS();

    String response = buildWsResponse("edge_mixer.save", requestId, [](JsonObject& data) {
        data["saved"] = true;
    });
    client->text(response);
}

void registerWsEdgeMixerCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("edge_mixer.set", handleEdgeMixerSet);
    WsCommandRouter::registerCommand("edge_mixer.get", handleEdgeMixerGet);
    WsCommandRouter::registerCommand("edge_mixer.save", handleEdgeMixerSave);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
