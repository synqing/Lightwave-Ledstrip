/**
 * @file SongAwareHandlers.h
 * @brief REST handlers for runtime-only song-aware director controls.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class SongAwareHandlers {
public:
    static void handleGetConfig(AsyncWebServerRequest* request);
    static void handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleGetStatus(AsyncWebServerRequest* request);
    static void handleGetAllowlist(AsyncWebServerRequest* request);
    static void handleSetAllowlist(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleResetAllowlist(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
