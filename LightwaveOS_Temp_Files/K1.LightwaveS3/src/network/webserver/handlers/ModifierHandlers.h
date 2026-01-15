/**
 * @file ModifierHandlers.h
 * @brief REST API handlers for effect modifiers
 *
 * Provides HTTP endpoints for adding, removing, and managing effect modifiers
 * that transform LED output in real-time.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>
#include <cstdint>

// Forward declarations
namespace lightwaveos {
namespace nodes {
class RendererNode;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Effect Modifier REST API handlers
 */
class ModifierHandlers {
public:
    /**
     * @brief Add a modifier to the stack
     * POST /api/v1/modifiers/add
     *
     * Request body (JSON):
     * {
     *   "type": "speed",           // speed, intensity, color_shift, mirror, glitch
     *   "multiplier": 2.0,         // SpeedModifier
     *   "baseIntensity": 0.5,      // IntensityModifier
     *   "source": "beat_phase",    // IntensityModifier
     *   "hueOffset": 128,          // ColorShiftModifier
     *   "mirrorAxis": "center",    // MirrorModifier
     *   "glitchAmount": 0.3        // GlitchModifier
     * }
     *
     * Response:
     * {
     *   "success": true,
     *   "data": {"modifierId": 0},
     *   "timestamp": 12345,
     *   "version": "1.0"
     * }
     */
    static void handleAddModifier(
        AsyncWebServerRequest* request,
        uint8_t* data,
        size_t len,
        nodes::RendererNode* renderer
    );

    /**
     * @brief Remove a modifier from the stack
     * POST /api/v1/modifiers/remove
     *
     * Request body (JSON):
     * {
     *   "type": "speed"  // Modifier type to remove
     * }
     */
    static void handleRemoveModifier(
        AsyncWebServerRequest* request,
        uint8_t* data,
        size_t len,
        nodes::RendererNode* renderer
    );

    /**
     * @brief List all active modifiers
     * GET /api/v1/modifiers/list
     *
     * Response:
     * {
     *   "success": true,
     *   "data": {
     *     "modifiers": [
     *       {"type": "speed", "name": "Speed", "enabled": true},
     *       {"type": "intensity", "name": "Intensity", "enabled": true}
     *     ],
     *     "count": 2,
     *     "maxCount": 8
     *   }
     * }
     */
    static void handleListModifiers(
        AsyncWebServerRequest* request,
        nodes::RendererNode* renderer
    );

    /**
     * @brief Clear all modifiers from stack
     * POST /api/v1/modifiers/clear
     */
    static void handleClearModifiers(
        AsyncWebServerRequest* request,
        nodes::RendererNode* renderer
    );

    /**
     * @brief Update modifier parameters
     * POST /api/v1/modifiers/update
     *
     * Request body (JSON):
     * {
     *   "type": "speed",
     *   "multiplier": 1.5
     * }
     */
    static void handleUpdateModifier(
        AsyncWebServerRequest* request,
        uint8_t* data,
        size_t len,
        nodes::RendererNode* renderer
    );
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
