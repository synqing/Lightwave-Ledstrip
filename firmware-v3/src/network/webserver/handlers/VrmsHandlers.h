/**
 * @file VrmsHandlers.h
 * @brief VRMS perceptual metrics HTTP handler
 */

#pragma once

#include "../../../config/features.h"

#if FEATURE_VRMS_METRICS

#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace actors { class RendererActor; }
namespace network {
namespace webserver {
namespace handlers {

class VrmsHandlers {
public:
    static void handleGetVrms(AsyncWebServerRequest* request,
                               lightwaveos::actors::RendererActor* renderer);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif  // FEATURE_VRMS_METRICS
