// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
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
#include "../../../config/version.h"
#include "../../../core/system/OtaLedFeedback.h"
#include "../../../core/system/OtaSessionLock.h"
#include "../../../core/system/OtaTokenManager.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>
#include <Arduino.h>
// Base64 decoding - using mbedTLS (available in ESP32 Arduino core 6.x+)
#include <mbedtls/base64.h>

// Convenience aliases
using OtaLed = lightwaveos::core::system::OtaLedFeedback;
using OtaLock = lightwaveos::core::system::OtaSessionLock;
using OtaTransport = lightwaveos::core::system::OtaTransport;

#undef LW_LOG_TAG
#define LW_LOG_TAG "WsOta"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

// ============================================================================
// Thread-safe OTA session state
//
// These statics are accessed from:
//   - async_tcp task: WS command handlers (read/write)
//   - WiFiManager task: isWsOtaInProgress() (read)
//   - WsGateway disconnect handler: handleOtaClientDisconnect() (read/write)
//
// A portMUX_TYPE spinlock guards all reads and writes. Critical sections
// are kept SHORT (flag/counter access only -- never held during I/O).
// ============================================================================

static portMUX_TYPE s_wsOtaMux = portMUX_INITIALIZER_UNLOCKED;

// OTA session state (epoch-scoped: session is bound to client ID + connection epoch)
static bool s_otaSessionActive = false;
static uint32_t s_otaActiveClientId = 0;
static uint32_t s_otaActiveConnEpoch = 0;
static uint32_t s_otaTotalSize = 0;
static uint32_t s_otaBytesReceived = 0;
static uint32_t s_otaSessionStartTime = 0;
static uint32_t s_otaLastProgressPercent = 0;

// ============================================================================
// Spinlock-guarded state helpers
// ============================================================================

/// Read the active flag under spinlock
static bool readSessionActive() {
    bool active;
    taskENTER_CRITICAL(&s_wsOtaMux);
    active = s_otaSessionActive;
    taskEXIT_CRITICAL(&s_wsOtaMux);
    return active;
}

/// Read session active flag AND client ID atomically
static bool readSessionOwnership(uint32_t& outClientId) {
    bool active;
    taskENTER_CRITICAL(&s_wsOtaMux);
    active = s_otaSessionActive;
    outClientId = s_otaActiveClientId;
    taskEXIT_CRITICAL(&s_wsOtaMux);
    return active;
}

/// Activate session state under spinlock (called after Update.begin succeeds)
static void activateSession(uint32_t clientId, uint32_t totalSize) {
    taskENTER_CRITICAL(&s_wsOtaMux);
    s_otaSessionActive = true;
    s_otaActiveClientId = clientId;
    s_otaActiveConnEpoch = 0;
    s_otaTotalSize = totalSize;
    s_otaBytesReceived = 0;
    s_otaSessionStartTime = millis();
    s_otaLastProgressPercent = 0;
    taskEXIT_CRITICAL(&s_wsOtaMux);
}

/// Clear all session state under spinlock
static void clearSessionState() {
    taskENTER_CRITICAL(&s_wsOtaMux);
    s_otaSessionActive = false;
    s_otaActiveClientId = 0;
    s_otaActiveConnEpoch = 0;
    s_otaBytesReceived = 0;
    s_otaTotalSize = 0;
    s_otaLastProgressPercent = 0;
    taskEXIT_CRITICAL(&s_wsOtaMux);
}

/// Snapshot of chunk-relevant state for use outside the spinlock
struct ChunkStateSnapshot {
    bool active;
    uint32_t clientId;
    uint32_t bytesReceived;
    uint32_t totalSize;
    uint32_t lastProgressPercent;
};

/// Read chunk-relevant state atomically
static ChunkStateSnapshot readChunkState() {
    ChunkStateSnapshot snap;
    taskENTER_CRITICAL(&s_wsOtaMux);
    snap.active = s_otaSessionActive;
    snap.clientId = s_otaActiveClientId;
    snap.bytesReceived = s_otaBytesReceived;
    snap.totalSize = s_otaTotalSize;
    snap.lastProgressPercent = s_otaLastProgressPercent;
    taskEXIT_CRITICAL(&s_wsOtaMux);
    return snap;
}

/// Update bytes received and progress after a successful chunk write
static void updateChunkProgress(uint32_t additionalBytes, uint32_t newProgressPercent) {
    taskENTER_CRITICAL(&s_wsOtaMux);
    s_otaBytesReceived += additionalBytes;
    if (newProgressPercent > s_otaLastProgressPercent) {
        s_otaLastProgressPercent = newProgressPercent;
    }
    taskEXIT_CRITICAL(&s_wsOtaMux);
}

/// Mark session inactive (on write failure) without clearing other state
static void markSessionInactive() {
    taskENTER_CRITICAL(&s_wsOtaMux);
    s_otaSessionActive = false;
    taskEXIT_CRITICAL(&s_wsOtaMux);
}

/// Read session start time (for telemetry duration calculation)
static uint32_t readSessionStartTime() {
    uint32_t t;
    taskENTER_CRITICAL(&s_wsOtaMux);
    t = s_otaSessionStartTime;
    taskEXIT_CRITICAL(&s_wsOtaMux);
    return t;
}

// Helper to emit telemetry for WS OTA events (forward declaration - no default args in forward decl)
static void emitOtaTelemetry(const char* eventType, const char* status,
                             uint32_t offset, uint32_t total,
                             const char* reason);

/**
 * @brief Constant-time token comparison to prevent timing side-channel attacks.
 *
 * Mirrors the pattern used in FirmwareHandlers::checkOTAToken() for REST OTA.
 * Even when lengths differ, a dummy loop over the expected token runs to avoid
 * leaking length information through timing.
 *
 * @param provided  Token string from the WebSocket message (may be nullptr)
 * @param expected  Expected token from NetworkConfig::OTA_UPDATE_TOKEN
 * @return true if tokens match
 */
static bool checkWsOtaToken(const char* provided, const char* expected) {
    size_t expectedLen = strlen(expected);

    if (provided == nullptr) {
        // Still iterate to keep timing consistent
        volatile uint8_t dummy = 0;
        for (size_t i = 0; i < expectedLen; i++) {
            dummy |= static_cast<uint8_t>(expected[i]);
        }
        (void)dummy;
        return false;
    }

    size_t providedLen = strlen(provided);

    if (providedLen != expectedLen) {
        volatile uint8_t dummy = 0;
        for (size_t i = 0; i < expectedLen; i++) {
            dummy |= static_cast<uint8_t>(expected[i]);
        }
        (void)dummy;
        return false;
    }

    volatile uint8_t result = 0;
    for (size_t i = 0; i < expectedLen; i++) {
        result |= static_cast<uint8_t>(provided[i]) ^ static_cast<uint8_t>(expected[i]);
    }

    return (result == 0);
}

// Helper to abort OTA session (called on disconnect or explicit abort)
// Reads state under spinlock, then performs I/O (telemetry, Update.abort, LED)
// outside the lock to keep critical sections short.
static void abortOtaSession(const char* reason) {
    uint32_t bytesReceived;
    uint32_t totalSize;
    bool wasActive;

    taskENTER_CRITICAL(&s_wsOtaMux);
    wasActive = s_otaSessionActive;
    bytesReceived = s_otaBytesReceived;
    totalSize = s_otaTotalSize;
    if (wasActive) {
        s_otaSessionActive = false;
        s_otaActiveClientId = 0;
        s_otaActiveConnEpoch = 0;
        s_otaBytesReceived = 0;
        s_otaTotalSize = 0;
        s_otaLastProgressPercent = 0;
    }
    taskEXIT_CRITICAL(&s_wsOtaMux);

    if (wasActive) {
        emitOtaTelemetry("ota.ws.failed", "failed", bytesReceived, totalSize, reason);
        Update.abort();
        OtaLed::showFailure();
        // Release cross-transport lock
        OtaLock::release();
    }
}

// Public API: check if a WebSocket OTA session is active (thread-safe)
bool isWsOtaInProgress() {
    return readSessionActive();
}

// Public API: called from WsGateway on client disconnect
void handleOtaClientDisconnect(uint32_t clientId) {
    // Check ownership under spinlock, abort if this client owns the session
    uint32_t ownerClientId;
    bool active = readSessionOwnership(ownerClientId);
    if (active && ownerClientId == clientId) {
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
        uint32_t sessionStart = readSessionStartTime();
        uint32_t duration = (tsMonoms >= sessionStart) ?
            (tsMonoms - sessionStart) : 0;
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
    } else if (strcmp(eventType, "ota.ws.auth_failed") == 0) {
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
            "{\"event\":\"ota.ws.auth_failed\",\"ts_mono_ms\":%lu,\"reason\":\"%s\","
            "\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            reasonEscaped
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
    } else if (strcmp(eventType, "ota.version.check") == 0 ||
               strcmp(eventType, "ota.version.downgrade_warning") == 0 ||
               strcmp(eventType, "ota.version.same_warning") == 0) {
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
            "{\"event\":\"%s\",\"ts_mono_ms\":%lu,\"currentVersion\":%lu,"
            "\"incomingVersion\":%lu,\"detail\":\"%s\",\"schemaVersion\":\"1.0.0\"}",
            eventType,
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(offset),   // repurposed: current version number
            static_cast<unsigned long>(total),     // repurposed: incoming version number
            reasonEscaped
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
            FIRMWARE_VERSION_STRING,
            FIRMWARE_VERSION_NUMBER,
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

    // Authenticate: require valid OTA token (constant-time comparison)
    // Uses per-device NVS token via OtaTokenManager (falls back to compile-time token)
    if (!checkWsOtaToken(req.token, lightwaveos::core::system::OtaTokenManager::getToken().c_str())) {
        emitOtaTelemetry("ota.ws.auth_failed", "auth_failed", 0, 0, "invalid_or_missing_token");
        client->text(buildWsError(ErrorCodes::UNAUTHORIZED,
            "Invalid or missing OTA token", requestId));
        return;
    }

    // ---- Version validation (informational / soft-reject) ----
    if (req.version && req.version[0] != '\0') {
        uint32_t incomingVersion = parseVersionNumber(req.version);
        uint32_t currentVersion = FIRMWARE_VERSION_NUMBER;

        // Always emit version check telemetry
        char versionDetail[96];
        snprintf(versionDetail, sizeof(versionDetail),
                 "current=%s(%lu) incoming=%s(%lu)",
                 FIRMWARE_VERSION_STRING,
                 static_cast<unsigned long>(currentVersion),
                 req.version,
                 static_cast<unsigned long>(incomingVersion));
        emitOtaTelemetry("ota.version.check", "info", currentVersion, incomingVersion, versionDetail);

        if (incomingVersion > 0) {
            if (incomingVersion < currentVersion) {
                // Downgrade detected
                snprintf(versionDetail, sizeof(versionDetail),
                         "downgrade %s(%lu)->%s(%lu) force=%s",
                         FIRMWARE_VERSION_STRING,
                         static_cast<unsigned long>(currentVersion),
                         req.version,
                         static_cast<unsigned long>(incomingVersion),
                         req.force ? "true" : "false");
                emitOtaTelemetry("ota.version.downgrade_warning", "warning",
                                 currentVersion, incomingVersion, versionDetail);

                if (!req.force) {
                    client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                        "Downgrade rejected: incoming version is older than running firmware. "
                        "Set force=true to override.", requestId));
                    return;
                }
            } else if (incomingVersion == currentVersion) {
                // Same version
                snprintf(versionDetail, sizeof(versionDetail),
                         "same version %s(%lu) force=%s",
                         FIRMWARE_VERSION_STRING,
                         static_cast<unsigned long>(currentVersion),
                         req.force ? "true" : "false");
                emitOtaTelemetry("ota.version.same_warning", "warning",
                                 currentVersion, incomingVersion, versionDetail);

                if (!req.force) {
                    client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                        "Same version: incoming firmware matches running version. "
                        "Set force=true to override.", requestId));
                    return;
                }
            }
            // incomingVersion > currentVersion: upgrade, always allowed
        }
        // If parseVersionNumber returned 0 (unparseable), skip validation and allow
    }
    // If no version field provided, proceed without validation (backward compatible)

    // Acquire cross-transport OTA lock (prevents concurrent REST + WS OTA)
    if (!OtaLock::tryAcquire(OtaTransport::WebSocket)) {
        client->text(buildWsError(ErrorCodes::BUSY,
            "Another OTA session is already active (REST or WebSocket)", requestId));
        return;
    }

    // Double-check local WS session state under spinlock
    if (readSessionActive()) {
        OtaLock::release();
        client->text(buildWsError(ErrorCodes::BUSY, "OTA session already active", requestId));
        return;
    }

    // Determine update type from target field
    const bool isFilesystem = (req.target && strcmp(req.target, "filesystem") == 0);
    const int updateCommand = isFilesystem ? U_SPIFFS : U_FLASH;
    const char* targetLabel = isFilesystem ? "Filesystem" : "Firmware";

    // Check available space (firmware uses sketch space; filesystem skips this check
    // because the ESP32 Update library validates against the partition table internally)
    if (!isFilesystem) {
        size_t freeSpace = ESP.getFreeSketchSpace();
        if (req.size > freeSpace) {
            OtaLock::release();
            String errorMsg = String(targetLabel) + " too large. Available: " +
                              String(freeSpace) + " bytes";
            client->text(buildWsError(ErrorCodes::INVALID_VALUE, errorMsg.c_str(), requestId));
            return;
        }
    }

    // Set MD5 checksum for verification if provided
    // Must be called BEFORE Update.begin() so the Update library can verify
    // the hash incrementally during writes and on Update.end(true)
    if (req.md5 && strlen(req.md5) == 32) {
        Update.setMD5(req.md5);
    } else if (req.md5 && strlen(req.md5) != 32) {
        OtaLock::release();
        client->text(buildWsError(ErrorCodes::INVALID_VALUE,
            "MD5 hash must be exactly 32 hex characters", requestId));
        return;
    }

    // Begin update (U_FLASH for firmware, U_SPIFFS for filesystem/LittleFS)
    if (!Update.begin(req.size, updateCommand)) {
        OtaLock::release();
        String errorMsg = "Update.begin(" + String(targetLabel) + ") failed: " +
                          String(Update.errorString());
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, errorMsg.c_str(), requestId));
        return;
    }

    // Initialize session state under spinlock (epoch-scoped: bound to this client)
    activateSession(client->id(), req.size);
    
    // Emit telemetry
    emitOtaTelemetry("ota.ws.begin", "begin", 0, req.size);

    // Show initial LED progress (0% - center LEDs only)
    OtaLed::showProgress(0);

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

    // Read session state atomically
    ChunkStateSnapshot snap = readChunkState();

    // Check session is active and owned by this client
    if (!snap.active) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "No active OTA session", requestId));
        return;
    }

    // Verify this client owns the active session (epoch-scoping)
    if (snap.clientId != client->id()) {
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

    // Write chunk to flash (offset should match bytesReceived from snapshot)
    if (req.offset != snap.bytesReceived) {
        free(decodedBuf);
        String errorMsg = "Offset mismatch. Expected: " + String(snap.bytesReceived) +
                         ", Got: " + String(req.offset);
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, errorMsg.c_str(), requestId));
        return;
    }

    // I/O: write to flash (NOT under spinlock)
    size_t written = Update.write(decodedBuf, decodedLen);
    free(decodedBuf);  // Free allocated buffer

    if (written != decodedLen) {
        String errorMsg = "Write failed. Expected: " + String(decodedLen) +
                         ", Written: " + String(written);
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, errorMsg.c_str(), requestId));
        emitOtaTelemetry("ota.ws.failed", "failed", snap.bytesReceived, snap.totalSize,
                        errorMsg.c_str());
        Update.abort();
        OtaLed::showFailure();
        markSessionInactive();
        OtaLock::release();
        return;
    }

    // Calculate new progress
    uint32_t newBytesReceived = snap.bytesReceived + written;
    uint8_t percent = (snap.totalSize > 0) ?
        ((newBytesReceived * 100) / snap.totalSize) : 0;

    // Determine if progress threshold crossed (before updating state)
    uint32_t newProgressPercent = snap.lastProgressPercent;
    bool shouldEmitProgress = (percent >= snap.lastProgressPercent + 10);
    if (shouldEmitProgress) {
        newProgressPercent = percent;
    }

    // Update state under spinlock
    updateChunkProgress(written, newProgressPercent);

    // Emit progress telemetry every 10% (outside spinlock)
    if (shouldEmitProgress) {
        emitOtaTelemetry("ota.ws.chunk", "chunk", newBytesReceived, snap.totalSize);

        // Update LED progress bar (every 10% to avoid slowing transfer)
        OtaLed::showProgress(percent);
    }

    // Send progress response
    uint8_t capturedPercent = percent;
    uint32_t capturedBytesReceived = newBytesReceived;
    uint32_t capturedTotalSize = snap.totalSize;
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
    
    if (!readSessionActive()) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "No active OTA session", reqId));
        return;
    }

    // Abort update and reset session state (also releases OtaSessionLock)
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

    // Read state atomically for validation
    ChunkStateSnapshot snap = readChunkState();

    // Check session is active and owned by this client
    if (!snap.active) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "No active OTA session", requestId));
        return;
    }

    // Verify this client owns the active session (epoch-scoping)
    if (snap.clientId != client->id()) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "OTA session owned by different client", requestId));
        return;
    }

    // Verify all bytes received
    if (snap.bytesReceived != snap.totalSize) {
        String errorMsg = "Incomplete transfer. Received: " + String(snap.bytesReceived) +
                         ", Expected: " + String(snap.totalSize);
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, errorMsg.c_str(), requestId));
        return;
    }

    // MD5 verification is handled automatically by the Update library:
    // Update.setMD5() is called in handleOtaBegin() when the client provides
    // an md5 hash. Update.end(true) below will fail if the computed MD5
    // does not match the expected hash, returning an error via Update.errorString().

    // Show full progress bar before verify
    OtaLed::showProgress(100);

    // Complete update (I/O -- NOT under spinlock)
    if (!Update.end(true)) {
        String errorMsg = "Update.end() failed: " + String(Update.errorString());
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, errorMsg.c_str(), requestId));
        emitOtaTelemetry("ota.ws.failed", "failed", snap.bytesReceived, snap.totalSize,
                        errorMsg.c_str());
        Update.abort();
        OtaLed::showFailure();
        clearSessionState();
        OtaLock::release();
        return;
    }

    // Emit complete telemetry
    emitOtaTelemetry("ota.ws.complete", "complete", snap.bytesReceived, snap.totalSize);

    // Show success LED feedback (green flashes)
    OtaLed::showSuccess();

    // Reset session state under spinlock (before reboot)
    clearSessionState();
    OtaLock::release();

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
