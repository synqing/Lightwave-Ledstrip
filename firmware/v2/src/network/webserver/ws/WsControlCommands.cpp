/**
 * @file WsControlCommands.cpp
 * @brief WebSocket control lease command handlers
 */

#include "WsControlCommands.h"

#if FEATURE_WEB_SERVER && FEATURE_CONTROL_LEASE

#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/system/ControlLeaseManager.h"
#include <ESPAsyncWebServer.h>
#include <Arduino.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using lightwaveos::core::system::ControlLeaseManager;

namespace {

void encodeLeaseState(JsonObject& data,
                      const ControlLeaseManager::LeaseState& state,
                      uint32_t remainingMs,
                      bool includeToken) {
    data["active"] = state.active;
    data["leaseId"] = state.leaseId;
    if (includeToken) {
        data["leaseToken"] = state.leaseToken;
    }
    data["scope"] = state.scope;
    data["ownerWsClientId"] = state.ownerWsClientId;
    data["ownerClientName"] = state.ownerClientName;
    data["ownerInstanceId"] = state.ownerInstanceId;
    data["ttlMs"] = state.ttlMs;
    data["heartbeatIntervalMs"] = state.heartbeatIntervalMs;
    data["acquiredAtMs"] = state.acquiredAtMs;
    data["expiresAtMs"] = state.expiresAtMs;
    data["remainingMs"] = remainingMs;
    data["takeoverAllowed"] = state.takeoverAllowed;
}

void sendControlError(AsyncWebSocketClient* client,
                      const char* requestId,
                      const char* code,
                      const char* message,
                      const ControlLeaseManager::LeaseState* state = nullptr,
                      uint32_t remainingMs = 0) {
    JsonDocument response;
    response["type"] = "error";
    if (requestId && requestId[0] != '\0') {
        response["requestId"] = requestId;
    }
    response["success"] = false;

    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = code;
    error["message"] = message;

    if (state) {
        error["ownerClientName"] = state->ownerClientName;
        error["remainingMs"] = remainingMs;
        error["scope"] = state->scope;
    }

    String output;
    serializeJson(response, output);
    client->text(output);
}

void handleControlAcquire(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";
    const char* clientName = doc["clientName"] | "Unknown";
    const char* clientInstanceId = doc["clientInstanceId"] | "";
    const char* scope = doc["scope"] | "global";

    const ControlLeaseManager::AcquireResult result = ControlLeaseManager::acquire(
        client->id(),
        clientName,
        clientInstanceId,
        scope
    );

    if (result.success) {
        String response = buildWsResponse("control.acquired", requestId, [&result](JsonObject& data) {
            encodeLeaseState(data, result.state, result.remainingMs, true);
        });
        client->text(response);
        return;
    }

    if (result.locked) {
        sendControlError(
            client,
            requestId,
            ErrorCodes::CONTROL_LOCKED,
            "Control lease is held by another client",
            &result.state,
            result.remainingMs
        );
        return;
    }

    sendControlError(client, requestId, ErrorCodes::INTERNAL_ERROR, "Failed to acquire control lease");
}

void handleControlHeartbeat(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";
    const char* leaseId = doc["leaseId"] | "";
    const char* leaseToken = doc["leaseToken"] | "";

    if (leaseId[0] == '\0') {
        sendControlError(client, requestId, ErrorCodes::MISSING_FIELD, "Missing leaseId");
        return;
    }
    if (leaseToken[0] == '\0') {
        sendControlError(client, requestId, ErrorCodes::MISSING_FIELD, "Missing leaseToken");
        return;
    }

    const ControlLeaseManager::HeartbeatResult result = ControlLeaseManager::heartbeat(
        client->id(),
        leaseId,
        leaseToken
    );

    if (result.success) {
        String response = buildWsResponse("control.heartbeatAck", requestId, [&result](JsonObject& data) {
            data["leaseId"] = result.state.leaseId;
            data["ttlMs"] = result.state.ttlMs;
            data["remainingMs"] = result.remainingMs;
            data["expiresAtMs"] = result.state.expiresAtMs;
        });
        client->text(response);
        return;
    }

    if (result.invalid) {
        sendControlError(client, requestId, ErrorCodes::LEASE_INVALID, "Lease token or lease ID is invalid");
        return;
    }
    if (result.expired) {
        sendControlError(client, requestId, ErrorCodes::LEASE_EXPIRED, "Control lease has expired");
        return;
    }
    if (result.locked) {
        sendControlError(
            client,
            requestId,
            ErrorCodes::CONTROL_LOCKED,
            "Control lease is held by another client",
            &result.state,
            result.remainingMs
        );
        return;
    }

    sendControlError(client, requestId, ErrorCodes::INTERNAL_ERROR, "Failed to process heartbeat");
}

void handleControlRelease(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";
    const char* leaseId = doc["leaseId"] | "";
    const char* leaseToken = doc["leaseToken"] | "";
    const char* reason = doc["reason"] | "user_release";

    if (leaseId[0] == '\0') {
        sendControlError(client, requestId, ErrorCodes::MISSING_FIELD, "Missing leaseId");
        return;
    }
    if (leaseToken[0] == '\0') {
        sendControlError(client, requestId, ErrorCodes::MISSING_FIELD, "Missing leaseToken");
        return;
    }

    const ControlLeaseManager::ReleaseResult result = ControlLeaseManager::release(
        client->id(),
        leaseId,
        leaseToken,
        reason
    );

    if (result.success) {
        String response = buildWsResponse("control.released", requestId, [&result, leaseId, reason](JsonObject& data) {
            data["released"] = result.released;
            data["leaseId"] = (result.state.leaseId.length() > 0) ? result.state.leaseId : String(leaseId);
            data["releasedAtMs"] = millis();
            data["reason"] = reason;
        });
        client->text(response);
        return;
    }

    if (result.invalid) {
        sendControlError(client, requestId, ErrorCodes::LEASE_INVALID, "Lease token or lease ID is invalid");
        return;
    }

    sendControlError(client, requestId, ErrorCodes::INTERNAL_ERROR, "Failed to release control lease");
}

void handleControlStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";
    ControlLeaseManager::LeaseState state = ControlLeaseManager::getState();
    const uint32_t remainingMs = ControlLeaseManager::getRemainingMs();

    String response = buildWsResponse("control.status", requestId, [&state, remainingMs](JsonObject& data) {
        encodeLeaseState(data, state, remainingMs, false);
    });
    client->text(response);
}

} // namespace

void registerWsControlCommands(const WebServerContext& ctx) {
    (void)ctx;
    WsCommandRouter::registerCommand("control.acquire", handleControlAcquire);
    WsCommandRouter::registerCommand("control.heartbeat", handleControlHeartbeat);
    WsCommandRouter::registerCommand("control.release", handleControlRelease);
    WsCommandRouter::registerCommand("control.status", handleControlStatus);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_CONTROL_LEASE
