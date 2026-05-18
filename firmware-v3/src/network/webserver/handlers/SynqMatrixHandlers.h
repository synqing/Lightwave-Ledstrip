/**
 * @file SynqMatrixHandlers.h
 * @brief REST handlers for runtime-only synq-matrix director controls.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class SynqMatrixHandlers {
public:
    static void handleGetConfig(AsyncWebServerRequest* request);
    static void handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleGetStatus(AsyncWebServerRequest* request);
    static void handleGetAllowlist(AsyncWebServerRequest* request);
    static void handleSetAllowlist(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleResetAllowlist(AsyncWebServerRequest* request);

    // Director boot preference (NVS-persisted: on|off, default off).
    // Independent of /config — config is runtime-only and resets on reboot.
    static void handleGetBoot(AsyncWebServerRequest* request);
    static void handleSetBoot(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // On-demand Director engage/release. Does NOT persist; the live owner is
    // updated and the user can persist the new state via handleSetBoot.
    static void handleEngage(AsyncWebServerRequest* request);
    static void handleRelease(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
