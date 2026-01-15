/**
 * @file ColorCorrectionHandlers.h
 * @brief REST API handlers for Color Correction Engine
 *
 * Provides HTTP endpoints for configuring the ColorCorrectionEngine,
 * which handles auto-exposure, white/brown guardrails, gamma correction,
 * and V-clamping for LED color quality.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>
#include <cstdint>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Color Correction Engine REST API handlers
 */
class ColorCorrectionHandlers {
public:
    /**
     * @brief Get current color correction configuration
     * GET /api/v1/colorCorrection/config
     *
     * Response:
     * {
     *   "success": true,
     *   "data": {
     *     "mode": 3,
     *     "autoExposureEnabled": true,
     *     "autoExposureTarget": 110,
     *     "brownGuardrailEnabled": true,
     *     "gammaEnabled": true,
     *     "gammaValue": 2.2,
     *     "vClampEnabled": true,
     *     "maxBrightness": 200,
     *     "hsvMinSaturation": 120,
     *     "rgbWhiteThreshold": 150,
     *     "rgbTargetMin": 100,
     *     "maxGreenPercentOfRed": 28,
     *     "maxBluePercentOfRed": 8,
     *     "saturationBoostAmount": 25
     *   }
     * }
     */
    static void handleGetConfig(AsyncWebServerRequest* request);

    /**
     * @brief Set color correction mode
     * POST /api/v1/colorCorrection/mode
     *
     * Request body (JSON):
     * {
     *   "mode": 3  // 0=OFF, 1=HSV, 2=RGB, 3=BOTH
     * }
     */
    static void handleSetMode(
        AsyncWebServerRequest* request,
        uint8_t* data,
        size_t len
    );

    /**
     * @brief Update color correction configuration
     * POST /api/v1/colorCorrection/config
     *
     * Request body (JSON) - any subset of parameters:
     * {
     *   "autoExposureEnabled": true,
     *   "autoExposureTarget": 110,
     *   "brownGuardrailEnabled": true,
     *   "gammaEnabled": true,
     *   "gammaValue": 2.2,
     *   "vClampEnabled": true,
     *   "maxBrightness": 200,
     *   "hsvMinSaturation": 120,
     *   "rgbWhiteThreshold": 150,
     *   "rgbTargetMin": 100,
     *   "maxGreenPercentOfRed": 28,
     *   "maxBluePercentOfRed": 8,
     *   "saturationBoostAmount": 25
     * }
     */
    static void handleSetConfig(
        AsyncWebServerRequest* request,
        uint8_t* data,
        size_t len
    );

    /**
     * @brief Save current configuration to NVS
     * POST /api/v1/colorCorrection/save
     *
     * Response:
     * {
     *   "success": true,
     *   "data": { "saved": true }
     * }
     */
    static void handleSave(AsyncWebServerRequest* request);

    /**
     * @brief Get available presets
     * GET /api/v1/colorCorrection/presets
     *
     * Response:
     * {
     *   "success": true,
     *   "data": {
     *     "presets": [
     *       { "id": 0, "name": "Off" },
     *       { "id": 1, "name": "Subtle" },
     *       { "id": 2, "name": "Balanced" },
     *       { "id": 3, "name": "Aggressive" }
     *     ],
     *     "currentPreset": 2
     *   }
     * }
     */
    static void handleGetPresets(AsyncWebServerRequest* request);

    /**
     * @brief Apply a preset
     * POST /api/v1/colorCorrection/preset
     *
     * Request body (JSON):
     * {
     *   "preset": 2,       // 0=Off, 1=Subtle, 2=Balanced, 3=Aggressive
     *   "save": true       // Optional: persist to NVS
     * }
     */
    static void handleSetPreset(
        AsyncWebServerRequest* request,
        uint8_t* data,
        size_t len
    );
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
