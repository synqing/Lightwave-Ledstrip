/**
 * @file WsSysCommands.cpp
 * @brief WebSocket system command handlers implementation
 *
 * Provides capability discovery for PRISM and other clients to determine
 * which features are available on this K1 device.
 */

#include "WsSysCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../../config/features.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

/**
 * @brief Handle sys.capabilities request
 *
 * Returns a JSON object indicating which features are available:
 *   - trinity: true if audio sync (Trinity protocol) is available
 *   - audio_sync: true if audio reactive effects are enabled
 *   - zones: true if zone system is enabled
 *   - ota: true if OTA updates are enabled
 *
 * This allows clients like PRISM to adapt their UI/behaviour based on
 * the device's capabilities.
 */
static void handleSysCapabilities(AsyncWebSocketClient* client,
                                  JsonDocument& doc,
                                  const WebServerContext& ctx) {
    JsonDocument response;
    response["type"] = "sys.capabilities";

    // Trinity/Audio Sync capability
#if FEATURE_AUDIO_SYNC
    response["trinity"] = true;
    response["audio_sync"] = true;
#else
    response["trinity"] = false;
    response["audio_sync"] = false;
#endif

    // Zone System capability
#if FEATURE_ZONE_SYSTEM
    response["zones"] = true;
#else
    response["zones"] = false;
#endif

    // OTA Update capability
#if FEATURE_OTA_UPDATE
    response["ota"] = true;
#else
    response["ota"] = false;
#endif

    // Additional capabilities that clients may find useful
#if FEATURE_TRANSITIONS
    response["transitions"] = true;
#else
    response["transitions"] = false;
#endif

#if FEATURE_PATTERN_REGISTRY
    response["pattern_registry"] = true;
#else
    response["pattern_registry"] = false;
#endif

    // Serialise and send
    char buf[256];
    serializeJson(response, buf, sizeof(buf));
    client->text(buf);
}

void registerWsSysCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("sys.capabilities", handleSysCapabilities);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
