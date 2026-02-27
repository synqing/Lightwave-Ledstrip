/**
 * @file AuthHandlers.cpp
 * @brief API Key authentication management handlers implementation
 */

#include "AuthHandlers.h"

#if FEATURE_WEB_SERVER && FEATURE_API_AUTH

#define LW_LOG_TAG "AuthAPI"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void AuthHandlers::handleStatus(AsyncWebServerRequest* request, ApiKeyManager& keyManager) {
    // Public endpoint - no auth required
    LW_LOGI("Auth status check");

    sendSuccessResponse(request, [&keyManager](JsonObject& data) {
        data["enabled"] = true;  // Auth is enabled (this handler only exists when FEATURE_API_AUTH=1)
        data["keyConfigured"] = keyManager.hasCustomKey();
    });
}

void AuthHandlers::handleRotate(AsyncWebServerRequest* request, ApiKeyManager& keyManager) {
    // Note: Auth check is done in V1ApiRoutes before this handler is called
    LW_LOGI("API key rotation requested");

    String newKey = keyManager.generateKey();
    if (newKey.isEmpty()) {
        LW_LOGE("Failed to generate new API key");
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to generate new key");
        return;
    }

    LW_LOGI("New API key generated successfully");

    sendSuccessResponse(request, [&newKey](JsonObject& data) {
        data["key"] = newKey;
        data["message"] = "Store this key securely. It will not be shown again.";
    });
}

void AuthHandlers::handleClear(AsyncWebServerRequest* request, ApiKeyManager& keyManager) {
    // Note: Auth check is done in V1ApiRoutes before this handler is called
    LW_LOGI("API key clear requested");

    if (!keyManager.clearKey()) {
        LW_LOGE("Failed to clear API key from NVS");
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to clear key from NVS");
        return;
    }

    LW_LOGI("API key cleared, reverting to compile-time default");

    sendSuccessResponse(request, [](JsonObject& data) {
        data["message"] = "Key cleared. Now using compile-time default key.";
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_API_AUTH
