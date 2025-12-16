#ifndef API_RESPONSE_H
#define API_RESPONSE_H

/**
 * @file ApiResponse.h
 * @brief Standardized API response helpers for LightwaveOS v1 API
 *
 * Provides consistent response formatting for both success and error cases.
 * All responses include success flag, timestamp, and version.
 *
 * Response Format:
 * Success: {"success": true, "data": {...}, "timestamp": 1702771200, "version": "1.0"}
 * Error:   {"success": false, "error": {"code": "...", "message": "...", "field": "..."}, ...}
 */

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <functional>

// API version
#define API_VERSION "1.0"

// ============================================================================
// Error Codes (constexpr strings stored in flash)
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
}

// HTTP Status Codes for convenience
namespace HttpStatus {
    constexpr uint16_t OK = 200;
    constexpr uint16_t CREATED = 201;
    constexpr uint16_t ACCEPTED = 202;
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
 * @brief Send a standardized success response with optional data
 * @param request The async web request to respond to
 * @param data Optional JSON document containing response data
 */
inline void sendSuccessResponse(AsyncWebServerRequest* request,
                                 const JsonDocument& data) {
    StaticJsonDocument<512> response;
    response["success"] = true;
    if (!data.isNull() && data.size() > 0) {
        response["data"] = data;
    }
    response["timestamp"] = millis();
    response["version"] = API_VERSION;

    String output;
    serializeJson(response, output);
    request->send(HttpStatus::OK, "application/json", output);
}

/**
 * @brief Send a standardized success response with no data
 * @param request The async web request to respond to
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
 * @param request The async web request to respond to
 * @param builder Function that populates the data object
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
 * @param request The async web request to respond to
 * @param builder Function that populates the data object
 * @param bufferSize Size of JSON buffer (for large responses)
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
 * @param request The async web request to respond to
 * @param httpCode HTTP status code
 * @param errorCode Machine-readable error code from ErrorCodes namespace
 * @param message Human-readable error message
 * @param field Optional field name that caused the error
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

/**
 * @brief Send a standardized error response with additional details
 * @param request The async web request to respond to
 * @param httpCode HTTP status code
 * @param errorCode Machine-readable error code
 * @param message Human-readable error message
 * @param detailsBuilder Function that populates error details
 */
inline void sendErrorResponseWithDetails(AsyncWebServerRequest* request,
                                          uint16_t httpCode,
                                          const char* errorCode,
                                          const char* message,
                                          std::function<void(JsonObject&)> detailsBuilder) {
    StaticJsonDocument<384> response;
    response["success"] = false;

    JsonObject error = response.createNestedObject("error");
    error["code"] = errorCode;
    error["message"] = message;
    JsonObject details = error.createNestedObject("details");
    detailsBuilder(details);

    response["timestamp"] = millis();
    response["version"] = API_VERSION;

    String output;
    serializeJson(response, output);
    request->send(httpCode, "application/json", output);
}

// ============================================================================
// Legacy Response Helpers (for backward compatibility)
// ============================================================================

/**
 * @brief Send a legacy-format success response
 * @param request The async web request to respond to
 *
 * Returns: {"status": "ok"}
 * Used by legacy /api/* endpoints for backward compatibility
 */
inline void sendLegacySuccess(AsyncWebServerRequest* request) {
    request->send(HttpStatus::OK, "application/json", "{\"status\":\"ok\"}");
}

/**
 * @brief Send a legacy-format error response
 * @param request The async web request to respond to
 * @param message Error message
 * @param httpCode HTTP status code (default 400)
 *
 * Returns: {"error": "message"}
 * Used by legacy /api/* endpoints for backward compatibility
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
 * @param responseType The response type (e.g., "response", "error")
 * @param requestId Optional request ID for request-response correlation
 * @param builder Function that populates the data object
 * @return JSON string ready to send via WebSocket
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
 * @param errorCode Machine-readable error code
 * @param message Human-readable error message
 * @param requestId Optional request ID for correlation
 * @return JSON string ready to send via WebSocket
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

#endif // API_RESPONSE_H
