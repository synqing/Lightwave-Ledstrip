/**
 * @file ControlHandlers.cpp
 * @brief Control lease HTTP handlers implementation
 */

#include "ControlHandlers.h"
#include "../../../config/features.h"

#if FEATURE_CONTROL_LEASE
#include "../../../core/system/ControlLeaseManager.h"
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void ControlHandlers::handleStatus(AsyncWebServerRequest* request) {
#if FEATURE_CONTROL_LEASE
    using lightwaveos::core::system::ControlLeaseManager;
    const ControlLeaseManager::LeaseState state = ControlLeaseManager::getState();
    const uint32_t remainingMs = ControlLeaseManager::getRemainingMs();

    sendSuccessResponse(request, [&state, remainingMs](JsonObject& data) {
        data["active"] = state.active;
        data["leaseId"] = state.leaseId;
        data["scope"] = state.scope;
        data["ownerClientName"] = state.ownerClientName;
        data["ownerInstanceId"] = state.ownerInstanceId;
        data["ownerWsClientId"] = state.ownerWsClientId;
        data["remainingMs"] = remainingMs;
        data["ttlMs"] = state.ttlMs;
        data["heartbeatIntervalMs"] = state.heartbeatIntervalMs;
        data["takeoverAllowed"] = state.takeoverAllowed;
    });
#else
    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      ErrorCodes::FEATURE_DISABLED, "Control lease is disabled");
#endif
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

