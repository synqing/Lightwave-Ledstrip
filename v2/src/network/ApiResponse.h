/**
 * @file ApiResponse.h
 * @brief Standardized API response helpers for LightwaveOS v2 API
 *
 * Provides consistent response formatting for both success and error cases.
 * All responses include success flag, timestamp, and version.
 *
 * Response Format:
 * Success: {"success": true, "data": {...}, "timestamp": 1702771200, "version": "2.0"}
 * Error:   {"success": false, "error": {"code": "...", "message": "...", "field": "..."}, ...}
 */

#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <functional>

namespace lightwaveos {
namespace network {

// API version
constexpr const char* API_VERSION = "2.0";

// ============================================================================
// Error Codes (constexpr strings)
// ============================================================================

namespace ErrorCodes {
    constexpr const char* INVALID_JSON = "INVALID_JSON";
    constexpr const char* MISSING_FIELD = "MISSING_FIELD";
    constexpr const char* INVALID_VALUE = "INVALID_VALUE";
    constexpr const char* INVALID_TYPE = "INVALID_TYPE";
    constexpr const char* OUT_OF_RANGE = "OUT_OF_RANGE";
    constexpr const char* UNAUTHORIZED = "UNAUTHORIZED";
    constexpr const char* RATE_LIMITED = "RATE_LIMITED";
    constexpr const char* INTERNAL_ERROR = "INTERNAL_ERROR";
    constexpr const char* NOT_FOUND = "NOT_FOUND";
    constexpr const char* BUSY = "BUSY";
    constexpr const char* CONNECTION_LIMIT = "CONNECTION_LIMIT";
    constexpr const char* FEATURE_DISABLED = "FEATURE_DISABLED";
    constexpr const char* SYSTEM_NOT_READY = "SYSTEM_NOT_READY";
}

// HTTP Status Codes
namespace HttpStatus {
    constexpr uint16_t OK = 200;
    constexpr uint16_t CREATED = 201;
    constexpr uint16_t ACCEPTED = 202;
    constexpr uint16_t NO_CONTENT = 204;
    constexpr uint16_t BAD_REQUEST = 400;
    constexpr uint16_t UNAUTHORIZED = 401;
    constexpr uint16_t FORBIDDEN = 403;
    constexpr uint16_t NOT_FOUND = 404;
    constexpr uint16_t TOO_MANY_REQUESTS = 429;
    constexpr uint16_t INTERNAL_ERROR = 500;
    constexpr uint16_t SERVICE_UNAVAILABLE = 503;
}

// ============================================================================
// Response Helpers
// ============================================================================

/**
 * @brief Send a standardized success response with no data
 */
inline void sendSuccessResponse(AsyncWebServerRequest* request) {
    StaticJsonDocument<128> response;
    response["success"] = true;
    response["timestamp"] = millis();
    response["version"] = API_VERSION;

    String output;
    serializeJson(response, output);
    request->send(HttpStatus::OK, "application/json", output);
}

/**
 * @brief Send a standardized success response using a builder function
 */
inline void sendSuccessResponse(AsyncWebServerRequest* request,
                                 std::function<void(JsonObject&)> builder) {
    StaticJsonDocument<512> response;
    response["success"] = true;
    JsonObject data = response.createNestedObject("data");
    builder(data);
    response["timestamp"] = millis();
    response["version"] = API_VERSION;

    String output;
    serializeJson(response, output);
    request->send(HttpStatus::OK, "application/json", output);
}

/**
 * @brief Send a standardized success response with large data buffer
 */
inline void sendSuccessResponseLarge(AsyncWebServerRequest* request,
                                      std::function<void(JsonObject&)> builder,
                                      size_t bufferSize = 1024) {
    DynamicJsonDocument response(bufferSize);
    response["success"] = true;
    JsonObject data = response.createNestedObject("data");
    builder(data);
    response["timestamp"] = millis();
    response["version"] = API_VERSION;

    String output;
    serializeJson(response, output);
    request->send(HttpStatus::OK, "application/json", output);
}

/**
 * @brief Send a standardized error response
 */
inline void sendErrorResponse(AsyncWebServerRequest* request,
                               uint16_t httpCode,
                               const char* errorCode,
                               const char* message,
                               const char* field = nullptr) {
    StaticJsonDocument<256> response;
    response["success"] = false;

    JsonObject error = response.createNestedObject("error");
    error["code"] = errorCode;
    error["message"] = message;
    if (field != nullptr) {
        error["field"] = field;
    }

    response["timestamp"] = millis();
    response["version"] = API_VERSION;

    String output;
    serializeJson(response, output);
    request->send(httpCode, "application/json", output);
}

// ============================================================================
// Legacy Response Helpers (backward compatibility)
// ============================================================================

/**
 * @brief Send a legacy-format success response
 * Returns: {"status": "ok"}
 */
inline void sendLegacySuccess(AsyncWebServerRequest* request) {
    request->send(HttpStatus::OK, "application/json", "{\"status\":\"ok\"}");
}

/**
 * @brief Send a legacy-format error response
 * Returns: {"error": "message"}
 */
inline void sendLegacyError(AsyncWebServerRequest* request,
                             const char* message,
                             uint16_t httpCode = HttpStatus::BAD_REQUEST) {
    StaticJsonDocument<128> doc;
    doc["error"] = message;
    String output;
    serializeJson(doc, output);
    request->send(httpCode, "application/json", output);
}

// ============================================================================
// WebSocket Response Helpers
// ============================================================================

/**
 * @brief Build a standardized WebSocket response
 */
inline String buildWsResponse(const char* responseType,
                               const char* requestId,
                               std::function<void(JsonObject&)> builder) {
    StaticJsonDocument<512> response;
    response["type"] = responseType;
    if (requestId != nullptr && strlen(requestId) > 0) {
        response["requestId"] = requestId;
    }
    response["success"] = true;
    JsonObject data = response.createNestedObject("data");
    builder(data);

    String output;
    serializeJson(response, output);
    return output;
}

/**
 * @brief Build a standardized WebSocket error response
 */
inline String buildWsError(const char* errorCode,
                            const char* message,
                            const char* requestId = nullptr) {
    StaticJsonDocument<256> response;
    response["type"] = "error";
    if (requestId != nullptr && strlen(requestId) > 0) {
        response["requestId"] = requestId;
    }
    response["success"] = false;
    JsonObject error = response.createNestedObject("error");
    error["code"] = errorCode;
    error["message"] = message;

    String output;
    serializeJson(response, output);
    return output;
}

} // namespace network
} // namespace lightwaveos
