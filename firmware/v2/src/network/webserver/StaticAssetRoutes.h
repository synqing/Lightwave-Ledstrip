/**
 * @file StaticAssetRoutes.h
 * @brief Static asset route registration
 *
 * Handles serving static files from LittleFS (dashboard, assets, favicon).
 * Also provides SPA fallback routing for non-API paths.
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

