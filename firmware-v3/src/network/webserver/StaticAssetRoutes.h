/**
 * @file StaticAssetRoutes.h
 * @brief Inline launcher page and fallback route registration
 *
 * Serves a minimal device-info HTML page at the root URL with runtime
 * values (IP, SSID, heap, uptime) injected via snprintf().  No LittleFS
 * or external asset files are required.
 */

#pragma once

#include "HttpRouteRegistry.h"

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief Static asset route registrar
 */
class StaticAssetRoutes {
public:
    /**
     * @brief Register all static asset routes
     * @param registry Route registry
     */
    static void registerRoutes(HttpRouteRegistry& registry);
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

