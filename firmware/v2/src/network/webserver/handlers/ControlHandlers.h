/**
 * @file ControlHandlers.h
 * @brief Control lease HTTP handlers
 */

#pragma once

#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class ControlHandlers {
public:
    static void handleStatus(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

