/**
 * @file WsFilesystemCommands.cpp
 * @brief WebSocket filesystem command handlers implementation
 */

#include "WsFilesystemCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"
#include <LittleFS.h>

#define LW_LOG_TAG "WS-FS"
#include "../../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

static void handleFilesystemStatus(AsyncWebSocketClient* client, JsonDocument& doc, const lightwaveos::network::webserver::WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.server) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "WebServer instance not available", requestId));
        return;
    }

    String response = buildWsResponse("filesystem.status", requestId, [&ctx](JsonObject& data) {
        WebServer* server = const_cast<WebServer*>(ctx.server);
        bool mounted = server->isLittleFSMounted();
        data["mounted"] = mounted;

        if (mounted) {
            // Get filesystem info if mounted (ESP32 uses totalBytes/usedBytes directly)
            size_t totalBytes = LittleFS.totalBytes();
            size_t usedBytes = LittleFS.usedBytes();
            
            data["totalBytes"] = totalBytes;
            data["usedBytes"] = usedBytes;
            data["freeBytes"] = totalBytes - usedBytes;
        } else {
            data["totalBytes"] = 0;
            data["usedBytes"] = 0;
            data["freeBytes"] = 0;
        }
    });
    client->text(response);
}

static void handleFilesystemMount(AsyncWebSocketClient* client, JsonDocument& doc, const lightwaveos::network::webserver::WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.server) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "WebServer instance not available", requestId));
        return;
    }

    WebServer* server = const_cast<WebServer*>(ctx.server);
    bool mounted = server->mountLittleFS();

    if (mounted) {
        String response = buildWsResponse("filesystem.mount", requestId, [](JsonObject& data) {
            data["mounted"] = true;
            data["message"] = "Filesystem mounted successfully";
        });
        client->text(response);
        LW_LOGI("Filesystem mounted via WebSocket");
    } else {
        client->text(buildWsError(ErrorCodes::OPERATION_FAILED, "Filesystem mount failed", requestId));
        LW_LOGE("Filesystem mount failed via WebSocket");
    }
}

static void handleFilesystemUnmount(AsyncWebSocketClient* client, JsonDocument& doc, const lightwaveos::network::webserver::WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.server) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "WebServer instance not available", requestId));
        return;
    }

    WebServer* server = const_cast<WebServer*>(ctx.server);
    bool unmounted = server->unmountLittleFS();

    if (unmounted) {
        String response = buildWsResponse("filesystem.unmount", requestId, [](JsonObject& data) {
            data["mounted"] = false;
            data["message"] = "Filesystem unmounted successfully";
        });
        client->text(response);
        LW_LOGI("Filesystem unmounted via WebSocket");
    } else {
        client->text(buildWsError(ErrorCodes::OPERATION_FAILED, "Cannot unmount filesystem while WebServer is running", requestId));
        LW_LOGW("Filesystem unmount blocked via WebSocket (server running)");
    }
}

static void handleFilesystemRestart(AsyncWebSocketClient* client, JsonDocument& doc, const lightwaveos::network::webserver::WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.server) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "WebServer instance not available", requestId));
        return;
    }

    WebServer* server = const_cast<WebServer*>(ctx.server);
    
    bool wasMounted = server->isLittleFSMounted();
    bool unmounted = server->unmountLittleFS();
    
    // If filesystem was mounted and is still running, unmount will fail
    if (wasMounted && !unmounted) {
        client->text(buildWsError(ErrorCodes::OPERATION_FAILED, 
                                  "Cannot restart filesystem while server is running - unmount failed", 
                                  requestId));
        LW_LOGW("Filesystem restart blocked via WebSocket (server running)");
        return;
    }
    
    // Attempt to mount
    bool mounted = server->mountLittleFS();

    if (mounted) {
        String response = buildWsResponse("filesystem.restart", requestId, [wasMounted, unmounted](JsonObject& data) {
            data["mounted"] = true;
            if (wasMounted && unmounted) {
                data["message"] = "Filesystem restarted successfully (unmounted and remounted)";
                data["action"] = "restarted";
            } else if (!wasMounted) {
                data["message"] = "Filesystem mounted successfully (was not previously mounted)";
                data["action"] = "mounted";
            } else {
                data["message"] = "Filesystem mounted successfully";
                data["action"] = "mounted";
            }
        });
        client->text(response);
        LW_LOGI("Filesystem restarted via WebSocket (wasMounted=%s, unmounted=%s, mounted=yes)",
                wasMounted ? "yes" : "no", unmounted ? "yes" : "no");
    } else {
        client->text(buildWsError(ErrorCodes::OPERATION_FAILED, 
                                  "Filesystem restart failed - mount failed", 
                                  requestId));
        LW_LOGE("Filesystem restart failed via WebSocket (mount failed)");
    }
}

void registerWsFilesystemCommands(const lightwaveos::network::webserver::WebServerContext& ctx) {
    WsCommandRouter::registerCommand("filesystem.status", handleFilesystemStatus);
    WsCommandRouter::registerCommand("filesystem.mount", handleFilesystemMount);
    WsCommandRouter::registerCommand("filesystem.unmount", handleFilesystemUnmount);
    WsCommandRouter::registerCommand("filesystem.restart", handleFilesystemRestart);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
