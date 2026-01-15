/**
 * @file FilesystemHandlers.cpp
 * @brief Filesystem management handlers implementation
 */

#include "FilesystemHandlers.h"
#include "../../WebServer.h"
#include "../../ApiResponse.h"
#include <LittleFS.h>

#define LW_LOG_TAG "Filesystem"
#include "../../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void FilesystemHandlers::handleFilesystemStatus(AsyncWebServerRequest* request, WebServer* server) {
    if (!server) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "WebServer instance not available");
        return;
    }

    sendSuccessResponse(request, [server](JsonObject& data) {
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
}

void FilesystemHandlers::handleFilesystemMount(AsyncWebServerRequest* request, WebServer* server) {
    if (!server) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "WebServer instance not available");
        return;
    }

    if (server->mountLittleFS()) {
        sendSuccessResponse(request, [](JsonObject& data) {
            data["mounted"] = true;
            data["message"] = "Filesystem mounted successfully";
        });
        LW_LOGI("Filesystem mounted via API");
    } else {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::OPERATION_FAILED, "Filesystem mount failed");
        LW_LOGE("Filesystem mount failed via API");
    }
}

void FilesystemHandlers::handleFilesystemUnmount(AsyncWebServerRequest* request, WebServer* server) {
    if (!server) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "WebServer instance not available");
        return;
    }

    if (server->unmountLittleFS()) {
        sendSuccessResponse(request, [](JsonObject& data) {
            data["mounted"] = false;
            data["message"] = "Filesystem unmounted successfully";
        });
        LW_LOGI("Filesystem unmounted via API");
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OPERATION_FAILED, "Cannot unmount filesystem while WebServer is running");
        LW_LOGW("Filesystem unmount blocked via API (server running)");
    }
}

void FilesystemHandlers::handleFilesystemRestart(AsyncWebServerRequest* request, WebServer* server) {
    if (!server) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "WebServer instance not available");
        return;
    }

    bool wasMounted = server->isLittleFSMounted();
    bool unmounted = server->unmountLittleFS();
    
    // If filesystem was mounted and is still running, unmount will fail
    // In this case, we can't restart - return appropriate error
    if (wasMounted && !unmounted) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OPERATION_FAILED, 
                          "Cannot restart filesystem while server is running - unmount failed");
        LW_LOGW("Filesystem restart blocked - server must be stopped first");
        return;
    }
    
    // Attempt to mount
    bool mounted = server->mountLittleFS();

    if (mounted) {
        sendSuccessResponse(request, [wasMounted, unmounted](JsonObject& data) {
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
        LW_LOGI("Filesystem restart successful (wasMounted=%s, unmounted=%s, mounted=yes)",
                wasMounted ? "yes" : "no", unmounted ? "yes" : "no");
    } else {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::OPERATION_FAILED, 
                          "Filesystem restart failed - mount failed");
        LW_LOGE("Filesystem restart failed - mount failed");
    }
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
