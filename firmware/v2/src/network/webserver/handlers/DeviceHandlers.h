/**
 * @file DeviceHandlers.h
 * @brief Device status and info HTTP handlers
 *
 * Extracted from WebServer for better separation of concerns.
 */

#pragma once

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>

// Forward declarations
namespace lightwaveos {
namespace nodes {
class RendererNode;
class NodeOrchestrator;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Device-related HTTP handlers
 */
class DeviceHandlers {
public:
    /**
     * @brief Register device routes
     * @param registry Route registry
     * @param checkRateLimit Rate limit checker function
     */
    static void registerRoutes(HttpRouteRegistry& registry,
                                std::function<bool(AsyncWebServerRequest*)> checkRateLimit);
    
    /**
     * @brief Handle GET /api/v1/device/status
     */
    static void handleStatus(AsyncWebServerRequest* request, 
                             lightwaveos::nodes::NodeOrchestrator& orchestrator, 
                             lightwaveos::nodes::RendererNode* renderer,
                             uint32_t startTime, bool apMode, size_t wsClientCount);
    
    /**
     * @brief Handle GET /api/v1/device/info
     */
    static void handleInfo(AsyncWebServerRequest* request, 
                           lightwaveos::nodes::NodeOrchestrator& orchestrator, 
                           lightwaveos::nodes::RendererNode* renderer);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

