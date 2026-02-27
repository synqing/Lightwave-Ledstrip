// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AuthHandlers.h
 * @brief API Key authentication management handlers for LightwaveOS v2
 *
 * Provides REST endpoints for API key management:
 * - GET /api/v1/auth/status - Public status check (enabled/configured)
 * - POST /api/v1/auth/rotate - Generate new key (requires valid key)
 * - DELETE /api/v1/auth/key - Clear NVS key (requires valid key)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../../../config/features.h"

#if FEATURE_WEB_SERVER && FEATURE_API_AUTH

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include "../../ApiKeyManager.h"
#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class AuthHandlers {
public:
    /**
     * @brief GET /api/v1/auth/status - Public endpoint
     *
     * Returns authentication status without requiring valid key.
     * Response: {enabled: true, keyConfigured: bool}
     */
    static void handleStatus(AsyncWebServerRequest* request, ApiKeyManager& keyManager);

    /**
     * @brief POST /api/v1/auth/rotate - Generate new API key
     *
     * Requires valid X-API-Key header. Generates new random key,
     * saves to NVS, and returns the new key.
     *
     * WARNING: The new key is only returned ONCE. Store it securely.
     *
     * Response: {key: "LW-XXXX-XXXX-...", message: "Store this key securely"}
     */
    static void handleRotate(AsyncWebServerRequest* request, ApiKeyManager& keyManager);

    /**
     * @brief DELETE /api/v1/auth/key - Clear NVS key
     *
     * Requires valid X-API-Key header. Clears the custom key from NVS,
     * reverting to compile-time default.
     *
     * Response: {message: "Key cleared, using compile-time default"}
     */
    static void handleClear(AsyncWebServerRequest* request, ApiKeyManager& keyManager);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_API_AUTH
