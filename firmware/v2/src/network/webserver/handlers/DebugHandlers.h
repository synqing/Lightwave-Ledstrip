/**
 * @file DebugHandlers.h
 * @brief Debug-related HTTP handlers for audio verbosity control
 *
 * Provides REST API endpoints for runtime control of audio debug verbosity.
 * All endpoints are guarded by FEATURE_AUDIO_SYNC.
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "../../../config/features.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Debug-related HTTP handlers
 *
 * Endpoints:
 *   GET  /api/v1/debug/audio - Get current verbosity settings
 *   POST /api/v1/debug/audio - Set verbosity level and/or base interval
 */
class DebugHandlers {
public:
#if FEATURE_AUDIO_SYNC
    /**
     * @brief Handle GET /api/v1/debug/audio
     *
     * Returns current audio debug configuration:
     * - verbosity (0-5)
     * - baseInterval (frames between logs)
     * - derived intervals for each verbosity level
     */
    static void handleAudioDebugGet(AsyncWebServerRequest* request);

    /**
     * @brief Handle POST /api/v1/debug/audio
     *
     * Sets audio debug configuration.
     * Request body:
     * {
     *   "verbosity": 0-5,        // optional
     *   "baseInterval": 1-1000   // optional
     * }
     */
    static void handleAudioDebugSet(AsyncWebServerRequest* request,
                                     uint8_t* data, size_t len);
#endif
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
