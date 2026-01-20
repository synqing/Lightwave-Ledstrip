/**
 * @file WsOtaCommands.cpp
 * @brief WebSocket OTA command handlers implementation
 */

#include "WsOtaCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../codec/WsOtaCodec.h"
#include "../../../config/network_config.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>
#include <Arduino.h>
// Base64 decoding - using mbedTLS (available in ESP32 Arduino core 6.x+)
#include <mbedtls/base64.h>

#undef LW_LOG_TAG
#define LW_LOG_TAG "WsOta"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

// OTA session state (epoch-scoped: session is bound to client ID + connection epoch)
static bool s_otaSessionActive = false;
static uint32_t s_otaActiveClientId = 0;
static uint32_t s_otaActiveConnEpoch = 0;
static uint32_t s_otaTotalSize = 0;
static uint32_t s_otaBytesReceived = 0;
static uint32_t s_otaSessionStartTime = 0;
static uint32_t s_otaLastProgressPercent = 0;

// Helper to emit telemetry for WS OTA events (forward declaration - no default args in forward decl)
static void emitOtaTelemetry(const char* eventType, const char* status,
                             uint32_t offset, uint32_t total,
                             const char* reason);

// Helper to abort OTA session (called on disconnect or explicit abort)
static void abortOtaSession(const char* reason) {
    if (s_otaSessionActive) {
        emitOtaTelemetry("ota.ws.failed", "failed", s_otaBytesReceived, s_otaTotalSize, reason);
        Update.abort();
        s_otaSessionActive = false;
        s_otaActiveClientId = 0;
        s_otaActiveConnEpoch = 0;
        s_otaBytesReceived = 0;
        s_otaTotalSize = 0;
        s_otaLastProgressPercent = 0;
    }
}

// Public API: called from WsGateway on client disconnect
void handleOtaClientDisconnect(uint32_t clientId) {
    // Abort OTA if this client owns the active session
    if (s_otaSessionActive && s_otaActiveClientId == clientId) {
        abortOtaSession("ws_disconnect");
    }
}

// Helper to emit telemetry for WS OTA events
static void emitOtaTelemetry(const char* eventType, const char* status,
                             uint32_t offset = 0, uint32_t total = 0,
                             const char* reason = nullptr) {
    uint32_t tsMonoms = millis();
    char buf[512];
    
    if (strcmp(eventType, "ota.ws.begin") == 0) {
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.ws.begin\",\"ts_mono_ms\":%lu,\"totalBytes\":%lu,"
            "\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(total)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    } else if (strcmp(eventType, "ota.ws.chunk") == 0) {
        uint8_t percent = (total > 0) ? ((offset * 100) / total) : 0;
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.ws.chunk\",\"ts_mono_ms\":%lu,\"offset\":%lu,"
            "\"totalBytes\":%lu,\"percent\":%u,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(offset),
            static_cast<unsigned long>(total),
            percent
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    } else if (strcmp(eventType, "ota.ws.complete") == 0) {
        uint32_t duration = (tsMonoms >= s_otaSessionStartTime) ? 
            (tsMonoms - s_otaSessionStartTime) : 0;
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.ws.complete\",\"ts_mono_ms\":%lu,\"duration\":%lu,"
            "\"finalSize\":%lu,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(duration),
            static_cast<unsigned long>(offset)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    } else if (strcmp(eventType, "ota.ws.failed") == 0) {
        char reasonEscaped[128] = {0};
        if (reason) {
            size_t outIdx = 0;
            for (size_t i = 0; reason[i] != '\0' && outIdx < sizeof(reasonEscaped) - 1; i++) {
                if (reason[i] == '"') {
                    if (outIdx < sizeof(reasonEscaped) - 2) {
                        reasonEscaped[outIdx++] = '\\';
                        reasonEscaped[outIdx++] = '"';
                    }
                } else if (reason[i] == '\\') {
                    if (outIdx < sizeof(reasonEscaped) - 2) {
                        reasonEscaped[outIdx++] = '\\';
                        reasonEscaped[outIdx++] = '\\';
                    }
                } else if (reason[i] >= 32 && reason[i] < 127) {
                    reasonEscaped[outIdx++] = reason[i];
                }
            }
            reasonEscaped[outIdx] = '\0';
        }
        
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.ws.failed\",\"ts_mono_ms\":%lu,\"reason\":\"%s\","
            "\"bytesReceived\":%lu,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            reasonEscaped,
            static_cast<unsigned long>(offset)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    } else if (strcmp(eventType, "ota.ws.abort") == 0) {
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.ws.abort\",\"ts_mono_ms\":%lu,\"bytesReceived\":%lu,"
            "\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(offset)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    }
}

static void handleOtaCheck(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::OtaCheckDecodeResult decodeResult = codec::WsOtaCodec::decodeOtaCheck(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    // Return OTA status (version, space available, etc.)
    String response = buildWsResponse("ota.status", requestId, [](JsonObject& data) {
        codec::WsOtaCodec::encodeOtaStatus(
            data,
            "2.0.0",  // TODO: Get actual firmware version
            ESP.getSketchSize(),
            ESP.getFreeSketchSpace(),
            ESP.getFreeSketchSpace() > 0
        );
    });
    client->text(response);
}

static void handleOtaBegin(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::OtaBeginDecodeResult decodeResult = codec::WsOtaCodec::decodeOtaBegin(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::OtaBeginRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    
    // Check if another OTA session is active
    if (s_otaSessionActive) {
        client->text(buildWsError(ErrorCodes::BUSY, "OTA session already active", requestId));
        return;
    }
    
    // Check available space
    size_t freeSpace = ESP.getFreeSketchSpace();
    if (req.size > freeSpace) {
        String errorMsg = "Firmware too large. Available: " + String(freeSpace) + " bytes";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, errorMsg.c_str(), requestId));
        return;
    }
    
    // Begin update
    if (!Update.begin(req.size, U_FLASH)) {
        String errorMsg = "Update.begin() failed: " + String(Update.errorString());
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, errorMsg.c_str(), requestId));
        return;
    }
    
    // Initialize session state (epoch-scoped: bound to this client)
    s_otaSessionActive = true;
    s_otaActiveClientId = client->id();
    s_otaActiveConnEpoch = 0;  // TODO: Get actual connEpoch from WsGateway if needed
    s_otaTotalSize = req.size;
    s_otaBytesReceived = 0;
    s_otaSessionStartTime = millis();
    s_otaLastProgressPercent = 0;
    
    // Emit telemetry
    emitOtaTelemetry("ota.ws.begin", "begin", 0, req.size);
    
    // Send ready response
    String response = buildWsResponse("ota.ready", requestId, [req](JsonObject& data) {
        codec::WsOtaCodec::encodeOtaReady(data, req.size);
    });
    client->text(response);
}

static void handleOtaChunk(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::OtaChunkDecodeResult decodeResult = codec::WsOtaCodec::decodeOtaChunk(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::OtaChunkRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    
    // Check session is active and owned by this client
    if (!s_otaSessionActive) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "No active OTA session", requestId));
        return;
    }
    
    // Verify this client owns the active session (epoch-scoping)
    if (s_otaActiveClientId != client->id()) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "OTA session owned by different client", requestId));
        return;
    }
    
    // Decode base64 data using mbedTLS
    size_t inputLen = strlen(req.data);
    size_t decodedLen = 0;
    // Calculate output buffer size (base64 is ~4/3 of input, but we need exact)
    size_t outputLen = (inputLen * 3) / 4 + 1;
    unsigned char* decodedBuf = (unsigned char*)malloc(outputLen);
    if (!decodedBuf) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Memory allocation failed", requestId));
        return;
    }
    
    int ret = mbedtls_base64_decode(decodedBuf, outputLen, &decodedLen,
                                    (const unsigned char*)req.data, inputLen);
    if (ret != 0 || decodedLen == 0) {
        free(decodedBuf);
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Invalid base64 data", requestId));
        return;
    }
    
    // Write chunk to flash (offset should match bytesReceived)
    if (req.offset != s_otaBytesReceived) {
        free(decodedBuf);
        String errorMsg = "Offset mismatch. Expected: " + String(s_otaBytesReceived) + 
                         ", Got: " + String(req.offset);
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, errorMsg.c_str(), requestId));
        return;
    }
    
    size_t written = Update.write(decodedBuf, decodedLen);
    free(decodedBuf);  // Free allocated buffer
    
    if (written != decodedLen) {
        String errorMsg = "Write failed. Expected: " + String(decodedLen) + 
                         ", Written: " + String(written);
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, errorMsg.c_str(), requestId));
        emitOtaTelemetry("ota.ws.failed", "failed", s_otaBytesReceived, s_otaTotalSize,
                        errorMsg.c_str());
        Update.abort();
        s_otaSessionActive = false;
        return;
    }
    
    s_otaBytesReceived += written;
    
    // Calculate progress
    uint8_t percent = (s_otaTotalSize > 0) ? 
        ((s_otaBytesReceived * 100) / s_otaTotalSize) : 0;
    
    // Emit progress telemetry every 10%
    if (percent >= s_otaLastProgressPercent + 10) {
        emitOtaTelemetry("ota.ws.chunk", "chunk", s_otaBytesReceived, s_otaTotalSize);
        s_otaLastProgressPercent = percent;
    }
    
    // Send progress response
    uint8_t capturedPercent = percent;
    uint32_t capturedBytesReceived = s_otaBytesReceived;
    uint32_t capturedTotalSize = s_otaTotalSize;
    String response = buildWsResponse("ota.progress", requestId, 
        [capturedPercent, capturedBytesReceived, capturedTotalSize](JsonObject& data) {
            codec::WsOtaCodec::encodeOtaProgress(data, capturedBytesReceived, 
                                                 capturedTotalSize, capturedPercent);
        });
    client->text(response);
}

static void handleOtaAbort(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    const char* requestId = nullptr;
    
    if (!codec::WsOtaCodec::decodeOtaAbort(root, &requestId)) {
        const char* reqId = requestId ? requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Invalid ota.abort request", reqId));
        return;
    }
    
    const char* reqId = requestId ? requestId : "";
    
    if (!s_otaSessionActive) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "No active OTA session", reqId));
        return;
    }
    
    // Abort update and reset session state
    abortOtaSession("user_abort");
    
    // Send response
    String response = buildWsResponse("ota.aborted", reqId, [](JsonObject& data) {
        codec::WsOtaCodec::encodeOtaAborted(data);
    });
    client->text(response);
}

static void handleOtaVerify(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::OtaVerifyDecodeResult decodeResult = codec::WsOtaCodec::decodeOtaVerify(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    // Check session is active and owned by this client
    if (!s_otaSessionActive) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "No active OTA session", requestId));
        return;
    }
    
    // Verify this client owns the active session (epoch-scoping)
    if (s_otaActiveClientId != client->id()) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "OTA session owned by different client", requestId));
        return;
    }
    
    // Verify all bytes received
    if (s_otaBytesReceived != s_otaTotalSize) {
        String errorMsg = "Incomplete transfer. Received: " + String(s_otaBytesReceived) + 
                         ", Expected: " + String(s_otaTotalSize);
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, errorMsg.c_str(), requestId));
        return;
    }
    
    // TODO: Verify MD5 if provided
    // if (decodeResult.request.md5) { ... }
    
    // Complete update
    if (!Update.end(true)) {
        String errorMsg = "Update.end() failed: " + String(Update.errorString());
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, errorMsg.c_str(), requestId));
        emitOtaTelemetry("ota.ws.failed", "failed", s_otaBytesReceived, s_otaTotalSize,
                        errorMsg.c_str());
        Update.abort();
        s_otaSessionActive = false;
        return;
    }
    
    // Emit complete telemetry
    emitOtaTelemetry("ota.ws.complete", "complete", s_otaBytesReceived, s_otaTotalSize);
    
    // Reset session state (before reboot)
    s_otaSessionActive = false;
    s_otaActiveClientId = 0;
    s_otaActiveConnEpoch = 0;
    s_otaBytesReceived = 0;
    s_otaTotalSize = 0;
    s_otaLastProgressPercent = 0;
    
    // Send complete response
    String response = buildWsResponse("ota.complete", requestId, [](JsonObject& data) {
        codec::WsOtaCodec::encodeOtaComplete(data, true);
    });
    client->text(response);
    
    // Reboot after short delay
    delay(500);
    ESP.restart();
}

void registerWsOtaCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("ota.check", handleOtaCheck);
    WsCommandRouter::registerCommand("ota.begin", handleOtaBegin);
    WsCommandRouter::registerCommand("ota.chunk", handleOtaChunk);
    WsCommandRouter::registerCommand("ota.abort", handleOtaAbort);
    WsCommandRouter::registerCommand("ota.verify", handleOtaVerify);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
