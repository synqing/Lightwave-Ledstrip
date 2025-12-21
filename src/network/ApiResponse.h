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

#include <Arduino.h>
#include <cJSON.h>
#include <functional>

// API version
#define API_VERSION "1.0"

// Forward declaration to avoid hard dependency on ESPAsyncWebServer.
// (The robust Phase 2 web backend uses esp_http_server instead.)
class AsyncWebServerRequest;

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

namespace ApiResponseInternal {
    inline void sendJson(AsyncWebServerRequest* request, uint16_t httpCode, cJSON* root) {
        // Always include timestamp/version if not already present
        if (!cJSON_HasObjectItem(root, "timestamp")) {
            cJSON_AddNumberToObject(root, "timestamp", millis());
        }
        if (!cJSON_HasObjectItem(root, "version")) {
            cJSON_AddStringToObject(root, "version", API_VERSION);
        }

        char* output = cJSON_PrintUnformatted(root);
        if (output == nullptr) {
            // Fallback minimal error (avoid recursion)
            request->send(HttpStatus::INTERNAL_ERROR, "application/json",
                          "{\"success\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"message\":\"JSON encode failed\"}}");
            return;
        }

        request->send(httpCode, "application/json", output);
        cJSON_free(output);
    }
} // namespace ApiResponseInternal

/**
 * @brief Send a standardized success response with optional data
 * @param request The async web request to respond to
 * @param dataBuilder Optional builder that populates the data object
 */
inline void sendSuccessResponse(AsyncWebServerRequest* request,
                                std::function<void(cJSON* data)> dataBuilder = nullptr) {
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);

    if (dataBuilder) {
        cJSON* data = cJSON_AddObjectToObject(response, "data");
        dataBuilder(data);
    }

    ApiResponseInternal::sendJson(request, HttpStatus::OK, response);
    cJSON_Delete(response);
}

/**
 * @brief Send a standardized success response with no data
 * @param request The async web request to respond to
 */
inline void sendSuccessResponseNoData(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, nullptr);
}

/**
 * @brief Send a standardized success response with large data buffer (legacy signature)
 * @param request The async web request to respond to
 * @param builder Function that populates the data object
 * @param bufferSize Ignored (cJSON is dynamic)
 */
inline void sendSuccessResponseLarge(AsyncWebServerRequest* request,
                                      std::function<void(cJSON* data)> builder,
                                      size_t bufferSize = 1024) {
    (void)bufferSize;
    sendSuccessResponse(request, builder);
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
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", false);

    cJSON* error = cJSON_AddObjectToObject(response, "error");
    cJSON_AddStringToObject(error, "code", errorCode ? errorCode : ErrorCodes::INTERNAL_ERROR);
    cJSON_AddStringToObject(error, "message", message ? message : "Error");
    if (field != nullptr) {
        cJSON_AddStringToObject(error, "field", field);
    }

    ApiResponseInternal::sendJson(request, httpCode, response);
    cJSON_Delete(response);
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
                                          std::function<void(cJSON* details)> detailsBuilder) {
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", false);

    cJSON* error = cJSON_AddObjectToObject(response, "error");
    cJSON_AddStringToObject(error, "code", errorCode ? errorCode : ErrorCodes::INTERNAL_ERROR);
    cJSON_AddStringToObject(error, "message", message ? message : "Error");
    cJSON* details = cJSON_AddObjectToObject(error, "details");
    if (detailsBuilder) {
        detailsBuilder(details);
    }

    ApiResponseInternal::sendJson(request, httpCode, response);
    cJSON_Delete(response);
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
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "error", message ? message : "Error");
    ApiResponseInternal::sendJson(request, httpCode, root);
    cJSON_Delete(root);
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
                               std::function<void(cJSON* data)> builder) {
    cJSON* response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "type", responseType ? responseType : "response");
    if (requestId != nullptr && strlen(requestId) > 0) {
        cJSON_AddStringToObject(response, "requestId", requestId);
    }
    cJSON_AddBoolToObject(response, "success", true);
    cJSON* data = cJSON_AddObjectToObject(response, "data");
    if (builder) {
        builder(data);
    }

    char* out = cJSON_PrintUnformatted(response);
    String output = out ? String(out) : String("{}");
    if (out) cJSON_free(out);
    cJSON_Delete(response);
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
    cJSON* response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "type", "error");
    if (requestId != nullptr && strlen(requestId) > 0) {
        cJSON_AddStringToObject(response, "requestId", requestId);
    }
    cJSON_AddBoolToObject(response, "success", false);
    cJSON* err = cJSON_AddObjectToObject(response, "error");
    cJSON_AddStringToObject(err, "code", errorCode ? errorCode : ErrorCodes::INTERNAL_ERROR);
    cJSON_AddStringToObject(err, "message", message ? message : "Error");

    char* out = cJSON_PrintUnformatted(response);
    String output = out ? String(out) : String("{}");
    if (out) cJSON_free(out);
    cJSON_Delete(response);
    return output;
}

#endif // API_RESPONSE_H
