/**
 * @file VrmsHandlers.cpp
 * @brief VRMS perceptual metrics HTTP handler implementation
 */

#include "VrmsHandlers.h"

#if FEATURE_VRMS_METRICS

#include "../../ApiResponse.h"
#include "../../../core/actors/RendererActor.h"
#include <ArduinoJson.h>

using namespace lightwaveos::actors;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void VrmsHandlers::handleGetVrms(AsyncWebServerRequest* request,
                                   RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                           "RENDERER_UNAVAILABLE", "Renderer not available");
        return;
    }

    metrics::VRMSVector v = renderer->getVrmsVector();

    sendSuccessResponse(request, [&v](JsonObject& data) {
        data["dominantHue"] = serialized(String(v.dominantHue, 1));
        data["colourVariance"] = serialized(String(v.colourVariance, 3));
        data["spatialCentroid"] = serialized(String(v.spatialCentroid, 1));
        data["symmetryScore"] = serialized(String(v.symmetryScore, 3));
        data["brightnessMean"] = serialized(String(v.brightnessMean, 1));
        data["brightnessVariance"] = serialized(String(v.brightnessVariance, 0));
        data["temporalFreq"] = serialized(String(v.temporalFreq, 3));
        data["audioVisualCorr"] = serialized(String(v.audioVisualCorr, 3));
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif  // FEATURE_VRMS_METRICS
