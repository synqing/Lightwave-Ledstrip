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
    constexpr const char* AUDIO_UNAVAILABLE = "AUDIO_UNAVAILABLE";
    constexpr const char* INVALID_ACTION = "INVALID_ACTION";
    constexpr const char* STORAGE_FULL = "STORAGE_FULL";
    constexpr const char* INVALID_PARAMETER = "INVALID_PARAMETER";
    constexpr const char* OPERATION_FAILED = "OPERATION_FAILED";
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
    constexpr uint16_t INSUFFICIENT_STORAGE = 507;
    constexpr uint16_t REQUEST_TIMEOUT = 408;
}

// ============================================================================
// Response Helpers
// ============================================================================

/**
 * @brief Send a standardized success response with no data
 */
inline void sendSuccessResponse(AsyncWebServerRequest* request) {
    JsonDocument response;
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
    JsonDocument response;
    response["success"] = true;
    JsonObject data = response["data"].to<JsonObject>();
    builder(data);
    response["timestamp"] = millis();
    response["version"] = API_VERSION;

    String output;
    serializeJson(response, output);
    request->send(HttpStatus::OK, "application/json", output);
}

/**
 * @brief Send a standardized success response with large data buffer
 * Note: bufferSize parameter is kept for API compatibility but is no longer used
 *       since ArduinoJson v7 uses dynamic allocation for JsonDocument
 */
inline void sendSuccessResponseLarge(AsyncWebServerRequest* request,
                                      std::function<void(JsonObject&)> builder,
                                      size_t bufferSize = 1024) {
    (void)bufferSize; // Unused in ArduinoJson v7
    JsonDocument response;
    response["success"] = true;
    JsonObject data = response["data"].to<JsonObject>();
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
    JsonDocument response;
    response["success"] = false;

    JsonObject error = response["error"].to<JsonObject>();
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
 * @brief Send a rate limit exceeded (429) error response with Retry-After header
 * @param request The HTTP request
 * @param retryAfterSeconds Seconds until client should retry (sent in Retry-After header)
 */
inline void sendRateLimitError(AsyncWebServerRequest* request, uint32_t retryAfterSeconds) {
    JsonDocument response;
    response["success"] = false;

    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = ErrorCodes::RATE_LIMITED;
    error["message"] = "Too many requests. Please wait before retrying.";
    error["retryAfter"] = retryAfterSeconds;

    response["timestamp"] = millis();
    response["version"] = API_VERSION;

    String output;
    serializeJson(response, output);

    // Create response with Retry-After header
    AsyncWebServerResponse* resp = request->beginResponse(HttpStatus::TOO_MANY_REQUESTS,
                                                           "application/json", output);
    char retryHeader[16];
    snprintf(retryHeader, sizeof(retryHeader), "%lu", (unsigned long)retryAfterSeconds);
    resp->addHeader("Retry-After", retryHeader);
    request->send(resp);
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
    JsonDocument response;
    response["type"] = responseType;
    if (requestId != nullptr && strlen(requestId) > 0) {
        response["requestId"] = requestId;
    }
    response["success"] = true;
    JsonObject data = response["data"].to<JsonObject>();
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
    JsonDocument response;
    response["type"] = "error";
    if (requestId != nullptr && strlen(requestId) > 0) {
        response["requestId"] = requestId;
    }
    response["success"] = false;
    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = errorCode;
    error["message"] = message;

    String output;
    serializeJson(response, output);
    return output;
}

/**
 * @brief Build a WebSocket rate limit error response with retry info
 */
inline String buildWsRateLimitError(uint32_t retryAfterSeconds, const char* requestId = nullptr) {
    JsonDocument response;
    response["type"] = "error";
    if (requestId != nullptr && strlen(requestId) > 0) {
        response["requestId"] = requestId;
    }
    response["success"] = false;
    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = ErrorCodes::RATE_LIMITED;
    error["message"] = "Too many messages. Please wait before retrying.";
    error["retryAfter"] = retryAfterSeconds;

    String output;
    serializeJson(response, output);
    return output;
}

// ============================================================================
// WebSocket Telemetry Helpers
// ============================================================================

namespace WsTelemetry {

/**
 * @brief Log msg.send telemetry event and send response via client->text()
 *
 * Extracts msgType and result from response JSON, logs structured JSONL event,
 * then sends response to client.
 *
 * @param client WebSocket client
 * @param response Response string (JSON)
 * @param responseType Optional response type (if null, extracted from response JSON)
 * @param clientId Client ID (from client->id())
 * @param connEpoch Connection epoch (0 if unknown)
 * @param eventSeq Event sequence number (monotonic)
 */
inline void sendWithLogging(AsyncWebSocketClient* client, const String& response, 
                            const char* responseType = nullptr,
                            uint32_t clientId = 0, uint32_t connEpoch = 0, uint32_t eventSeq = 0) {
    if (!client) return;
    
    // Get client ID if not provided
    if (clientId == 0) {
        clientId = client->id();
    }
    
    // Parse response to extract type and result
    const char* msgType = responseType ? responseType : "";
    const char* result = "ok";
    
    StaticJsonDocument<256> respDoc;
    DeserializationError error = deserializeJson(respDoc, response);
    if (!error) {
        if (!msgType || strlen(msgType) == 0) {
            msgType = respDoc["type"] | "";
        }
        
        if (respDoc.containsKey("error")) {
            result = "error";
        } else if (respDoc.containsKey("success")) {
            result = respDoc["success"].as<bool>() ? "ok" : "error";
        }
    }
    
    // Create bounded payload summary (~100 chars max)
    char payloadSummary[128] = {0};
    size_t len = response.length();
    if (len > sizeof(payloadSummary) - 1) {
        len = sizeof(payloadSummary) - 1;
    }
    memcpy(payloadSummary, response.c_str(), len);
    payloadSummary[len] = '\0';
    
    // Log structured msg.send event (JSONL)
    uint32_t tsMonoms = millis();
    {
        char buf[512];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"event\":\"msg.send\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"%s\",\"result\":\"%s\",\"payloadSummary\":\"%.100s\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId),
            msgType,
            result,
            payloadSummary
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    }
    
    // Send response
    client->text(response);
}

} // namespace WsTelemetry

} // namespace network
} // namespace lightwaveos
