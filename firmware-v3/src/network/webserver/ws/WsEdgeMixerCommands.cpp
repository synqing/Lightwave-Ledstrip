/**
 * @file WsEdgeMixerCommands.cpp
 * @brief WebSocket edge mixer command handlers implementation
 */

#include "WsEdgeMixerCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../effects/enhancement/EdgeMixer.h"
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
 * Accepts any combination of: mode (0-2), spread (0-60), strength (0-255).
 * Only provided fields are applied; omitted fields remain unchanged.
 */
static void handleEdgeMixerSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& mixer = EdgeMixer::getInstance();

    bool anySet = false;

    if (doc.containsKey("mode")) {
        uint8_t mode = doc["mode"] | 0;
        if (mode > 2) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "mode must be 0-2", requestId));
            return;
        }
        mixer.setMode(static_cast<EdgeMixerMode>(mode));
        anySet = true;
    }

    if (doc.containsKey("spread")) {
        uint8_t spread = doc["spread"] | 30;
        if (spread > 60) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "spread must be 0-60", requestId));
            return;
        }
        mixer.setSpread(spread);
        anySet = true;
    }

    if (doc.containsKey("strength")) {
        uint8_t strength = doc["strength"] | 255;
        mixer.setStrength(strength);
        anySet = true;
    }

    if (!anySet) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Provide mode, spread, or strength", requestId));
        return;
    }

    String response = buildWsResponse("edge_mixer.set", requestId, [&mixer](JsonObject& data) {
        data["mode"] = static_cast<uint8_t>(mixer.getMode());
        const char* modeNames[] = {"mirror", "analogous", "complementary"};
        data["modeName"] = modeNames[static_cast<uint8_t>(mixer.getMode())];
        data["spread"] = mixer.getSpread();
        data["strength"] = mixer.getStrength();
    });
    client->text(response);
}

/**
 * @brief Handle edge_mixer.get command
 *
 * Returns current mode, spread, and strength.
 */
static void handleEdgeMixerGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& mixer = EdgeMixer::getInstance();

    String response = buildWsResponse("edge_mixer.get", requestId, [&mixer](JsonObject& data) {
        data["mode"] = static_cast<uint8_t>(mixer.getMode());
        const char* modeNames[] = {"mirror", "analogous", "complementary"};
        data["modeName"] = modeNames[static_cast<uint8_t>(mixer.getMode())];
        data["spread"] = mixer.getSpread();
        data["strength"] = mixer.getStrength();
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
    EdgeMixer::getInstance().saveToNVS();

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
