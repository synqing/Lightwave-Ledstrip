// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file FirmwareHandlers.cpp
 * @brief Firmware/OTA update REST API handlers
 *
 * Implements multipart firmware upload via POST /api/v1/firmware/update (V1 API)
 * and POST /update (legacy route). Authentication is via X-OTA-Token header.
 *
 * Architecture notes:
 * - AsyncWebServer calls handleUpload() for each chunk of the multipart body.
 *   The upload handler CANNOT send HTTP responses.
 * - After all chunks are received, AsyncWebServer calls the request handler
 *   (handleV1Update or handleLegacyUpdate) which sends the final response.
 * - Static variables bridge state between the upload handler and request handler
 *   since they execute sequentially within the same connection context.
 * - Both routes share the same handleUpload() implementation.
 */

#include "FirmwareHandlers.h"
#include "../../ApiResponse.h"
#include "../../../config/network_config.h"
#include "../../../config/version.h"
#include "../../../core/system/OtaLedFeedback.h"
#include "../../../core/system/OtaSessionLock.h"
#include "../../../core/system/OtaTokenManager.h"
#include <Update.h>
#include <Arduino.h>

// Convenience aliases
using OtaLed = lightwaveos::core::system::OtaLedFeedback;
using OtaLock = lightwaveos::core::system::OtaSessionLock;
using OtaTransport = lightwaveos::core::system::OtaTransport;

// Use centralized version from version.h (backward compat for any code using FIRMWARE_VERSION)
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION FIRMWARE_VERSION_STRING
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// ============================================================================
// Thread-safe static upload state
//
// These statics are accessed from:
//   - async_tcp task: upload/request handlers (read/write)
//   - WiFiManager task: isRestOtaInProgress() (read)
//
// A portMUX_TYPE spinlock guards the "in progress" flag reads that cross
// task boundaries. The upload handlers themselves run sequentially on the
// async_tcp task, so intra-handler accesses during upload processing are
// inherently single-threaded. The spinlock protects ONLY the cross-task
// reads in isRestOtaInProgress().
// ============================================================================

static portMUX_TYPE s_restOtaMux = portMUX_INITIALIZER_UNLOCKED;

static bool s_updateStarted = false;     // Update.begin() was called successfully
static bool s_updateSuccess = false;     // Update.end(true) succeeded
static bool s_updateError = false;       // An error occurred during upload
static bool s_authFailed = false;        // Token validation failed on first chunk
static String s_errorMessage;            // Error detail for response
static uint32_t s_totalReceived = 0;     // Total bytes written so far
static uint32_t s_contentLength = 0;     // Expected total size
static uint32_t s_uploadStartTime = 0;   // millis() at upload start
static uint32_t s_lastProgressPercent = 0; // Last emitted progress percentage

// Filesystem OTA state — declared here (before isRestOtaInProgress) so both
// firmware and filesystem in-progress checks can be evaluated together.
// Full helpers (telemetry, reset, handlers) are defined after the firmware section.
static bool s_fsUpdateStarted = false;
static bool s_fsUpdateSuccess = false;
static bool s_fsUpdateError = false;
static bool s_fsAuthFailed = false;
static String s_fsErrorMessage;
static uint32_t s_fsTotalReceived = 0;
static uint32_t s_fsContentLength = 0;
static uint32_t s_fsUploadStartTime = 0;
static uint32_t s_fsLastProgressPercent = 0;

// ============================================================================
// Telemetry helpers
// ============================================================================

/**
 * @brief Emit structured telemetry JSON to Serial for OTA REST events.
 *
 * Event types: ota.rest.begin, ota.rest.progress, ota.rest.complete, ota.rest.failed
 */
static void emitRestOtaTelemetry(const char* eventType, uint32_t offset = 0,
                                  uint32_t total = 0, const char* reason = nullptr) {
    uint32_t tsMonoMs = millis();
    char buf[384];
    int n = 0;

    if (strcmp(eventType, "ota.rest.begin") == 0) {
        n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.rest.begin\",\"ts_mono_ms\":%lu,\"totalBytes\":%lu,"
            "\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoMs),
            static_cast<unsigned long>(total));
    } else if (strcmp(eventType, "ota.rest.progress") == 0) {
        uint8_t percent = (total > 0) ? static_cast<uint8_t>((offset * 100UL) / total) : 0;
        n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.rest.progress\",\"ts_mono_ms\":%lu,\"offset\":%lu,"
            "\"totalBytes\":%lu,\"percent\":%u,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoMs),
            static_cast<unsigned long>(offset),
            static_cast<unsigned long>(total),
            percent);
    } else if (strcmp(eventType, "ota.rest.complete") == 0) {
        uint32_t duration = (tsMonoMs >= s_uploadStartTime)
            ? (tsMonoMs - s_uploadStartTime) : 0;
        n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.rest.complete\",\"ts_mono_ms\":%lu,\"duration\":%lu,"
            "\"finalSize\":%lu,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoMs),
            static_cast<unsigned long>(duration),
            static_cast<unsigned long>(offset));
    } else if (strcmp(eventType, "ota.rest.failed") == 0) {
        // Escape reason string for JSON safety
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
        n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.rest.failed\",\"ts_mono_ms\":%lu,\"reason\":\"%s\","
            "\"bytesReceived\":%lu,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoMs),
            reasonEscaped,
            static_cast<unsigned long>(offset));
    }

    if (n > 0 && n < static_cast<int>(sizeof(buf))) {
        Serial.println(buf);
    }
}

/**
 * @brief Emit version comparison telemetry for REST OTA.
 *
 * Event types: ota.version.check, ota.version.downgrade_warning, ota.version.same_warning
 */
static void emitRestVersionTelemetry(const char* eventType, uint32_t currentVer,
                                      uint32_t incomingVer, const char* detail) {
    uint32_t tsMonoMs = millis();
    char detailEscaped[128] = {0};
    if (detail) {
        size_t outIdx = 0;
        for (size_t i = 0; detail[i] != '\0' && outIdx < sizeof(detailEscaped) - 1; i++) {
            if (detail[i] == '"') {
                if (outIdx < sizeof(detailEscaped) - 2) {
                    detailEscaped[outIdx++] = '\\';
                    detailEscaped[outIdx++] = '"';
                }
            } else if (detail[i] >= 32 && detail[i] < 127) {
                detailEscaped[outIdx++] = detail[i];
            }
        }
        detailEscaped[outIdx] = '\0';
    }
    char buf[384];
    int n = snprintf(buf, sizeof(buf),
        "{\"event\":\"%s\",\"ts_mono_ms\":%lu,\"currentVersion\":%lu,"
        "\"incomingVersion\":%lu,\"detail\":\"%s\",\"schemaVersion\":\"1.0.0\"}",
        eventType,
        static_cast<unsigned long>(tsMonoMs),
        static_cast<unsigned long>(currentVer),
        static_cast<unsigned long>(incomingVer),
        detailEscaped);
    if (n > 0 && n < static_cast<int>(sizeof(buf))) {
        Serial.println(buf);
    }
}

/**
 * @brief Reset all static upload state variables.
 * Called at the start of a new upload and after request completion.
 * Updates in-progress flags under spinlock for cross-task visibility.
 */
static void resetUploadState() {
    taskENTER_CRITICAL(&s_restOtaMux);
    s_updateStarted = false;
    s_updateSuccess = false;
    s_updateError = false;
    s_authFailed = false;
    taskEXIT_CRITICAL(&s_restOtaMux);
    s_errorMessage = String();
    s_totalReceived = 0;
    s_contentLength = 0;
    s_uploadStartTime = 0;
    s_lastProgressPercent = 0;
}

// ============================================================================
// OTA state query
// ============================================================================

bool FirmwareHandlers::isRestOtaInProgress() {
    bool active;
    taskENTER_CRITICAL(&s_restOtaMux);
    active = (s_updateStarted && !s_updateSuccess && !s_updateError) ||
             (s_fsUpdateStarted && !s_fsUpdateSuccess && !s_fsUpdateError);
    taskEXIT_CRITICAL(&s_restOtaMux);
    return active;
}

// ============================================================================
// Token validation
// ============================================================================

bool FirmwareHandlers::checkOTAToken(AsyncWebServerRequest* request) {
    using lightwaveos::core::system::OtaTokenManager;

    // Require X-OTA-Token header
    if (!request->hasHeader("X-OTA-Token")) {
        return false;
    }

    const String& provided = request->header("X-OTA-Token");
    const String& expectedStr = OtaTokenManager::getToken();
    const char* expected = expectedStr.c_str();
    size_t expectedLen = expectedStr.length();

    // Constant-time comparison to prevent timing side-channel attacks.
    // Even if lengths differ we still iterate over expectedLen to avoid
    // leaking length information through timing.
    if (provided.length() != expectedLen) {
        // Still do a dummy loop to keep timing consistent
        volatile uint8_t dummy = 0;
        for (size_t i = 0; i < expectedLen; i++) {
            dummy |= expected[i];
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

// ============================================================================
// Version endpoint (already implemented)
// ============================================================================

void FirmwareHandlers::handleVersion(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["version"] = FIRMWARE_VERSION;
        data["versionNumber"] = FIRMWARE_VERSION_NUMBER;
        data["platform"] = "ESP32-S3";
        data["status"] = "running";
        data["sketchSize"] = ESP.getSketchSize();
        data["freeSketchSpace"] = ESP.getFreeSketchSpace();
    });
}

// ============================================================================
// Upload handler (chunk processor)
// Called by AsyncWebServer for each chunk of multipart firmware upload.
// This handler CANNOT send HTTP responses -- only the request handlers can.
// ============================================================================

void FirmwareHandlers::handleUpload(AsyncWebServerRequest* request,
                                     const String& filename, size_t index,
                                     uint8_t* data, size_t len, bool final) {
    // --- First chunk (index == 0): initialize ---
    if (index == 0) {
        resetUploadState();
        s_uploadStartTime = millis();

        // Acquire cross-transport OTA lock (prevents concurrent REST + WS OTA).
        // We cannot send a response from here, so if acquisition fails we set
        // error flags that the request handler checks after upload completes.
        if (!OtaLock::tryAcquire(OtaTransport::Rest)) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_updateError = true;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_errorMessage = "Another OTA session is already active (REST or WebSocket)";
            emitRestOtaTelemetry("ota.rest.failed", 0, 0, "session_locked");
            return;
        }

        // Validate OTA token on the very first chunk.
        // We cannot send a response from here, so we set a flag that the
        // request handler checks when it runs after upload completes.
        if (!checkOTAToken(request)) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_authFailed = true;
            s_updateError = true;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_errorMessage = "Invalid or missing X-OTA-Token";
            emitRestOtaTelemetry("ota.rest.failed", 0, 0, "auth_failed");
            OtaLock::release();
            return;
        }

        // ---- Version validation via X-OTA-Version header ----
        // Informational: emits telemetry and soft-rejects if force is not set.
        // X-OTA-Force header controls whether downgrades/same-version are allowed.
        // If no X-OTA-Version header is present, validation is skipped (backward compatible).
        if (request->hasHeader("X-OTA-Version")) {
            const String& incomingVersionStr = request->header("X-OTA-Version");
            uint32_t incomingVersion = parseVersionNumber(incomingVersionStr.c_str());
            uint32_t currentVersion = FIRMWARE_VERSION_NUMBER;

            // Check for X-OTA-Force header (default: true for backward compatibility)
            bool forceUpdate = true;
            if (request->hasHeader("X-OTA-Force")) {
                const String& forceHeader = request->header("X-OTA-Force");
                forceUpdate = (forceHeader == "true" || forceHeader == "1");
            }

            char versionDetail[128];
            snprintf(versionDetail, sizeof(versionDetail),
                     "current=%s(%lu) incoming=%s(%lu) force=%s",
                     FIRMWARE_VERSION_STRING,
                     static_cast<unsigned long>(currentVersion),
                     incomingVersionStr.c_str(),
                     static_cast<unsigned long>(incomingVersion),
                     forceUpdate ? "true" : "false");
            emitRestVersionTelemetry("ota.version.check", currentVersion, incomingVersion, versionDetail);

            if (incomingVersion > 0) {
                if (incomingVersion < currentVersion) {
                    emitRestVersionTelemetry("ota.version.downgrade_warning",
                                              currentVersion, incomingVersion, versionDetail);
                    if (!forceUpdate) {
                        taskENTER_CRITICAL(&s_restOtaMux);
                        s_updateError = true;
                        taskEXIT_CRITICAL(&s_restOtaMux);
                        s_errorMessage = "Downgrade rejected: incoming version " +
                                         incomingVersionStr + " is older than running " +
                                         FIRMWARE_VERSION_STRING +
                                         ". Set X-OTA-Force: true to override.";
                        emitRestOtaTelemetry("ota.rest.failed", 0, 0, "version_downgrade_rejected");
                        OtaLock::release();
                        return;
                    }
                } else if (incomingVersion == currentVersion) {
                    emitRestVersionTelemetry("ota.version.same_warning",
                                              currentVersion, incomingVersion, versionDetail);
                    if (!forceUpdate) {
                        taskENTER_CRITICAL(&s_restOtaMux);
                        s_updateError = true;
                        taskEXIT_CRITICAL(&s_restOtaMux);
                        s_errorMessage = "Same version: incoming firmware matches running version " +
                                         String(FIRMWARE_VERSION_STRING) +
                                         ". Set X-OTA-Force: true to override.";
                        emitRestOtaTelemetry("ota.rest.failed", 0, 0, "version_same_rejected");
                        OtaLock::release();
                        return;
                    }
                }
                // incomingVersion > currentVersion: upgrade, always allowed
            }
            // If parseVersionNumber returned 0 (unparseable), skip validation and allow
        }

        // Determine firmware size from Content-Length header.
        // For multipart uploads, contentLength() returns the body size which
        // may include multipart framing overhead. We use UPDATE_SIZE_UNKNOWN
        // if the value seems unreliable, letting Update track it internally.
        s_contentLength = request->contentLength();

        size_t updateSize = (s_contentLength > 0) ? s_contentLength : UPDATE_SIZE_UNKNOWN;

        // Set MD5 checksum for verification if provided via X-OTA-MD5 header.
        // Must be called BEFORE Update.begin() so the Update library can verify
        // the hash incrementally during writes and on Update.end(true).
        if (request->hasHeader("X-OTA-MD5")) {
            const String& md5Header = request->header("X-OTA-MD5");
            if (md5Header.length() == 32) {
                Update.setMD5(md5Header.c_str());
            } else if (md5Header.length() > 0) {
                taskENTER_CRITICAL(&s_restOtaMux);
                s_updateError = true;
                taskEXIT_CRITICAL(&s_restOtaMux);
                s_errorMessage = "X-OTA-MD5 header must be exactly 32 hex characters";
                emitRestOtaTelemetry("ota.rest.failed", 0, s_contentLength,
                                     "invalid_md5_format");
                OtaLock::release();
                return;
            }
        }

        // Check available flash space
        size_t freeSpace = ESP.getFreeSketchSpace();
        if (s_contentLength > 0 && s_contentLength > freeSpace) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_updateError = true;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_errorMessage = "Firmware too large. Available: " + String(freeSpace) +
                             " bytes, Requested: " + String(s_contentLength) + " bytes";
            emitRestOtaTelemetry("ota.rest.failed", 0, s_contentLength,
                                 "firmware_too_large");
            OtaLock::release();
            return;
        }

        // Begin OTA update
        if (!Update.begin(updateSize, U_FLASH)) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_updateError = true;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_errorMessage = "Update.begin() failed: " + String(Update.errorString());
            emitRestOtaTelemetry("ota.rest.failed", 0, s_contentLength,
                                 Update.errorString());
            OtaLock::release();
            return;
        }

        taskENTER_CRITICAL(&s_restOtaMux);
        s_updateStarted = true;
        taskEXIT_CRITICAL(&s_restOtaMux);
        emitRestOtaTelemetry("ota.rest.begin", 0, s_contentLength);

        // Show initial LED progress (0% - center LEDs only)
        OtaLed::showProgress(0);
    }

    // --- If an error already occurred (auth failure, begin failure), skip writes ---
    if (s_updateError || s_authFailed) {
        return;
    }

    // --- Write chunk to flash ---
    if (s_updateStarted && len > 0) {
        size_t written = Update.write(data, len);
        if (written != len) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_updateError = true;
            s_updateStarted = false;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_errorMessage = "Flash write failed at offset " + String(s_totalReceived) +
                             ". Expected: " + String(len) +
                             ", Written: " + String(written) +
                             ". Error: " + String(Update.errorString());
            emitRestOtaTelemetry("ota.rest.failed", s_totalReceived, s_contentLength,
                                 "write_failed");
            Update.abort();
            OtaLed::showFailure();
            OtaLock::release();
            return;
        }

        s_totalReceived += written;

        // Emit progress telemetry every 10%
        if (s_contentLength > 0) {
            uint32_t percent = (s_totalReceived * 100UL) / s_contentLength;
            if (percent >= s_lastProgressPercent + 10) {
                emitRestOtaTelemetry("ota.rest.progress", s_totalReceived, s_contentLength);
                s_lastProgressPercent = percent;

                // Update LED progress bar (every 10% to avoid slowing transfer)
                OtaLed::showProgress(static_cast<uint8_t>(percent));
            }
        }
    }

    // --- Final chunk: finalize update ---
    if (final) {
        if (s_updateStarted) {
            // Show full progress bar before finalization
            OtaLed::showProgress(100);

            if (Update.end(true)) {
                taskENTER_CRITICAL(&s_restOtaMux);
                s_updateSuccess = true;
                taskEXIT_CRITICAL(&s_restOtaMux);
                emitRestOtaTelemetry("ota.rest.complete", s_totalReceived, s_contentLength);

                // Show success LED feedback (green flashes)
                OtaLed::showSuccess();
            } else {
                taskENTER_CRITICAL(&s_restOtaMux);
                s_updateError = true;
                taskEXIT_CRITICAL(&s_restOtaMux);
                s_errorMessage = "Update.end() failed: " + String(Update.errorString());
                emitRestOtaTelemetry("ota.rest.failed", s_totalReceived, s_contentLength,
                                     Update.errorString());

                // Show failure LED feedback (red flashes)
                OtaLed::showFailure();
            }
            // Release cross-transport lock (session is complete or failed)
            OtaLock::release();
        }
        // If update was never started (error on first chunk), lock was already released.
    }
}

// ============================================================================
// V1 API request handler (POST /api/v1/firmware/update)
// Called AFTER handleUpload() has processed all chunks.
// ============================================================================

void FirmwareHandlers::handleV1Update(AsyncWebServerRequest* request,
                                       std::function<bool(AsyncWebServerRequest*)> checkToken) {
    // Auth failure detected during upload
    if (s_authFailed) {
        sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                          ErrorCodes::UNAUTHORIZED,
                          "Invalid or missing X-OTA-Token header");
        resetUploadState();
        return;
    }

    // Upload error
    if (s_updateError) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR,
                          s_errorMessage.c_str());
        resetUploadState();
        return;
    }

    // Success
    if (s_updateSuccess) {
        uint32_t totalBytes = s_totalReceived;
        uint32_t duration = (millis() >= s_uploadStartTime)
            ? (millis() - s_uploadStartTime) : 0;

        sendSuccessResponse(request, [totalBytes, duration](JsonObject& data) {
            data["message"] = "Firmware updated successfully";
            data["rebooting"] = true;
            data["bytesWritten"] = totalBytes;
            data["durationMs"] = duration;
        });

        resetUploadState();

        // Schedule reboot after response is sent.
        // 500ms delay allows the TCP stack to flush the response.
        delay(500);
        ESP.restart();
        return;
    }

    // Fallback: no upload data received at all
    sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                      ErrorCodes::MISSING_FIELD,
                      "No firmware data received. Send a multipart/form-data POST "
                      "with the firmware binary.");
    resetUploadState();
}

// ============================================================================
// Legacy request handler (POST /update)
// Plain text response format for curl/scripting compatibility.
// ============================================================================

void FirmwareHandlers::handleLegacyUpdate(AsyncWebServerRequest* request,
                                           std::function<bool(AsyncWebServerRequest*)> checkToken) {
    // Auth failure
    if (s_authFailed) {
        request->send(HttpStatus::UNAUTHORIZED, "text/plain",
                      "ERROR: Invalid or missing X-OTA-Token header");
        resetUploadState();
        return;
    }

    // Upload error
    if (s_updateError) {
        request->send(HttpStatus::INTERNAL_ERROR, "text/plain",
                      "ERROR: " + s_errorMessage);
        resetUploadState();
        return;
    }

    // Success
    if (s_updateSuccess) {
        String msg = "OK - Firmware updated (" + String(s_totalReceived) +
                     " bytes), rebooting...";
        request->send(HttpStatus::OK, "text/plain", msg);

        resetUploadState();

        delay(500);
        ESP.restart();
        return;
    }

    // Fallback
    request->send(HttpStatus::BAD_REQUEST, "text/plain",
                  "ERROR: No firmware data received");
    resetUploadState();
}

// ============================================================================
// Filesystem OTA — Telemetry and handlers
// State variables declared at top of file alongside firmware OTA state.
// ============================================================================

/**
 * @brief Emit structured telemetry JSON to Serial for filesystem OTA REST events.
 *
 * Event types: ota.fs.begin, ota.fs.progress, ota.fs.complete, ota.fs.failed
 */
static void emitFsOtaTelemetry(const char* eventType, uint32_t offset = 0,
                                 uint32_t total = 0, const char* reason = nullptr) {
    uint32_t tsMonoMs = millis();
    char buf[384];
    int n = 0;

    if (strcmp(eventType, "ota.fs.begin") == 0) {
        n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.fs.begin\",\"ts_mono_ms\":%lu,\"totalBytes\":%lu,"
            "\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoMs),
            static_cast<unsigned long>(total));
    } else if (strcmp(eventType, "ota.fs.progress") == 0) {
        uint8_t percent = (total > 0) ? static_cast<uint8_t>((offset * 100UL) / total) : 0;
        n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.fs.progress\",\"ts_mono_ms\":%lu,\"offset\":%lu,"
            "\"totalBytes\":%lu,\"percent\":%u,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoMs),
            static_cast<unsigned long>(offset),
            static_cast<unsigned long>(total),
            percent);
    } else if (strcmp(eventType, "ota.fs.complete") == 0) {
        uint32_t duration = (tsMonoMs >= s_fsUploadStartTime)
            ? (tsMonoMs - s_fsUploadStartTime) : 0;
        n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.fs.complete\",\"ts_mono_ms\":%lu,\"duration\":%lu,"
            "\"finalSize\":%lu,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoMs),
            static_cast<unsigned long>(duration),
            static_cast<unsigned long>(offset));
    } else if (strcmp(eventType, "ota.fs.failed") == 0) {
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
        n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.fs.failed\",\"ts_mono_ms\":%lu,\"reason\":\"%s\","
            "\"bytesReceived\":%lu,\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoMs),
            reasonEscaped,
            static_cast<unsigned long>(offset));
    }

    if (n > 0 && n < static_cast<int>(sizeof(buf))) {
        Serial.println(buf);
    }
}

/**
 * @brief Reset all filesystem upload state variables.
 * Updates in-progress flags under spinlock for cross-task visibility.
 */
static void resetFsUploadState() {
    taskENTER_CRITICAL(&s_restOtaMux);
    s_fsUpdateStarted = false;
    s_fsUpdateSuccess = false;
    s_fsUpdateError = false;
    s_fsAuthFailed = false;
    taskEXIT_CRITICAL(&s_restOtaMux);
    s_fsErrorMessage = String();
    s_fsTotalReceived = 0;
    s_fsContentLength = 0;
    s_fsUploadStartTime = 0;
    s_fsLastProgressPercent = 0;
}

// ============================================================================
// Filesystem upload handler (chunk processor)
// Writes to U_SPIFFS partition (used by LittleFS).
// ============================================================================

void FirmwareHandlers::handleFsUpload(AsyncWebServerRequest* request,
                                       const String& filename, size_t index,
                                       uint8_t* data, size_t len, bool final) {
    // --- First chunk (index == 0): initialize ---
    if (index == 0) {
        resetFsUploadState();
        s_fsUploadStartTime = millis();

        // Acquire cross-transport OTA lock
        if (!OtaLock::tryAcquire(OtaTransport::Rest)) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_fsUpdateError = true;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_fsErrorMessage = "Another OTA session is already active (REST or WebSocket)";
            emitFsOtaTelemetry("ota.fs.failed", 0, 0, "session_locked");
            return;
        }

        // Validate OTA token on the very first chunk.
        if (!checkOTAToken(request)) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_fsAuthFailed = true;
            s_fsUpdateError = true;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_fsErrorMessage = "Invalid or missing X-OTA-Token";
            emitFsOtaTelemetry("ota.fs.failed", 0, 0, "auth_failed");
            OtaLock::release();
            return;
        }

        s_fsContentLength = request->contentLength();
        size_t updateSize = (s_fsContentLength > 0) ? s_fsContentLength : UPDATE_SIZE_UNKNOWN;

        // Set MD5 checksum for verification if provided via X-OTA-MD5 header.
        if (request->hasHeader("X-OTA-MD5")) {
            const String& md5Header = request->header("X-OTA-MD5");
            if (md5Header.length() == 32) {
                Update.setMD5(md5Header.c_str());
            } else if (md5Header.length() > 0) {
                taskENTER_CRITICAL(&s_restOtaMux);
                s_fsUpdateError = true;
                taskEXIT_CRITICAL(&s_restOtaMux);
                s_fsErrorMessage = "X-OTA-MD5 header must be exactly 32 hex characters";
                emitFsOtaTelemetry("ota.fs.failed", 0, s_fsContentLength,
                                    "invalid_md5_format");
                OtaLock::release();
                return;
            }
        }

        // Begin OTA update for filesystem partition (U_SPIFFS)
        if (!Update.begin(updateSize, U_SPIFFS)) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_fsUpdateError = true;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_fsErrorMessage = "Update.begin(U_SPIFFS) failed: " + String(Update.errorString());
            emitFsOtaTelemetry("ota.fs.failed", 0, s_fsContentLength,
                                Update.errorString());
            OtaLock::release();
            return;
        }

        taskENTER_CRITICAL(&s_restOtaMux);
        s_fsUpdateStarted = true;
        taskEXIT_CRITICAL(&s_restOtaMux);
        emitFsOtaTelemetry("ota.fs.begin", 0, s_fsContentLength);

        // Show initial LED progress (0% - center LEDs only)
        OtaLed::showProgress(0);
    }

    // --- If an error already occurred, skip writes ---
    if (s_fsUpdateError || s_fsAuthFailed) {
        return;
    }

    // --- Write chunk to flash ---
    if (s_fsUpdateStarted && len > 0) {
        size_t written = Update.write(data, len);
        if (written != len) {
            taskENTER_CRITICAL(&s_restOtaMux);
            s_fsUpdateError = true;
            s_fsUpdateStarted = false;
            taskEXIT_CRITICAL(&s_restOtaMux);
            s_fsErrorMessage = "Flash write failed at offset " + String(s_fsTotalReceived) +
                               ". Expected: " + String(len) +
                               ", Written: " + String(written) +
                               ". Error: " + String(Update.errorString());
            emitFsOtaTelemetry("ota.fs.failed", s_fsTotalReceived, s_fsContentLength,
                                "write_failed");
            Update.abort();
            OtaLed::showFailure();
            OtaLock::release();
            return;
        }

        s_fsTotalReceived += written;

        // Emit progress telemetry every 10%
        if (s_fsContentLength > 0) {
            uint32_t percent = (s_fsTotalReceived * 100UL) / s_fsContentLength;
            if (percent >= s_fsLastProgressPercent + 10) {
                emitFsOtaTelemetry("ota.fs.progress", s_fsTotalReceived, s_fsContentLength);
                s_fsLastProgressPercent = percent;
                OtaLed::showProgress(static_cast<uint8_t>(percent));
            }
        }
    }

    // --- Final chunk: finalize update ---
    if (final) {
        if (s_fsUpdateStarted) {
            OtaLed::showProgress(100);

            if (Update.end(true)) {
                taskENTER_CRITICAL(&s_restOtaMux);
                s_fsUpdateSuccess = true;
                taskEXIT_CRITICAL(&s_restOtaMux);
                emitFsOtaTelemetry("ota.fs.complete", s_fsTotalReceived, s_fsContentLength);
                OtaLed::showSuccess();
            } else {
                taskENTER_CRITICAL(&s_restOtaMux);
                s_fsUpdateError = true;
                taskEXIT_CRITICAL(&s_restOtaMux);
                s_fsErrorMessage = "Update.end() failed: " + String(Update.errorString());
                emitFsOtaTelemetry("ota.fs.failed", s_fsTotalReceived, s_fsContentLength,
                                    Update.errorString());
                OtaLed::showFailure();
            }
            // Release cross-transport lock (session is complete or failed)
            OtaLock::release();
        }
        // If update was never started (error on first chunk), lock was already released.
    }
}

// ============================================================================
// V1 API request handler (POST /api/v1/firmware/filesystem)
// Called AFTER handleFsUpload() has processed all chunks.
// ============================================================================

void FirmwareHandlers::handleV1FsUpdate(AsyncWebServerRequest* request,
                                         std::function<bool(AsyncWebServerRequest*)> checkToken) {
    // Auth failure detected during upload
    if (s_fsAuthFailed) {
        sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                          ErrorCodes::UNAUTHORIZED,
                          "Invalid or missing X-OTA-Token header");
        resetFsUploadState();
        return;
    }

    // Upload error
    if (s_fsUpdateError) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR,
                          s_fsErrorMessage.c_str());
        resetFsUploadState();
        return;
    }

    // Success
    if (s_fsUpdateSuccess) {
        uint32_t totalBytes = s_fsTotalReceived;
        uint32_t duration = (millis() >= s_fsUploadStartTime)
            ? (millis() - s_fsUploadStartTime) : 0;

        sendSuccessResponse(request, [totalBytes, duration](JsonObject& data) {
            data["message"] = "Filesystem updated successfully";
            data["rebooting"] = true;
            data["bytesWritten"] = totalBytes;
            data["durationMs"] = duration;
            data["target"] = "filesystem";
        });

        resetFsUploadState();

        // Schedule reboot after response is sent.
        // 500ms delay allows the TCP stack to flush the response.
        delay(500);
        ESP.restart();
        return;
    }

    // Fallback: no upload data received at all
    sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                      ErrorCodes::MISSING_FIELD,
                      "No filesystem data received. Send a multipart/form-data POST "
                      "with the LittleFS image binary.");
    resetFsUploadState();
}

// ============================================================================
// OTA Token management endpoints
// ============================================================================

void FirmwareHandlers::handleGetOtaToken(AsyncWebServerRequest* request) {
    using lightwaveos::core::system::OtaTokenManager;

    const String& token = OtaTokenManager::getToken();

    sendSuccessResponse(request, [&token](JsonObject& data) {
        data["token"] = token;
        data["source"] = OtaTokenManager::isUsingNvsToken() ? "nvs" : "compile_time";
        data["tokenLength"] = token.length();
    });
}

void FirmwareHandlers::handleSetOtaToken(AsyncWebServerRequest* request,
                                          uint8_t* data, size_t len) {
    using lightwaveos::core::system::OtaTokenManager;

    // Parse JSON body
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON,
                          "Invalid JSON body");
        return;
    }

    // Check for "action" field
    const char* action = doc["action"] | "regenerate";

    if (strcmp(action, "regenerate") == 0) {
        // Generate new random token
        if (OtaTokenManager::regenerateToken()) {
            const String& newToken = OtaTokenManager::getToken();
            sendSuccessResponse(request, [&newToken](JsonObject& respData) {
                respData["token"] = newToken;
                respData["source"] = "nvs";
                respData["action"] = "regenerated";
            });
        } else {
            sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                              ErrorCodes::INTERNAL_ERROR,
                              "Failed to regenerate OTA token");
        }
    } else if (strcmp(action, "set") == 0) {
        // Manually set token
        const char* newToken = doc["token"] | static_cast<const char*>(nullptr);
        if (newToken == nullptr || strlen(newToken) == 0) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD,
                              "Missing 'token' field for action 'set'",
                              "token");
            return;
        }

        if (OtaTokenManager::setToken(newToken)) {
            const String& currentToken = OtaTokenManager::getToken();
            sendSuccessResponse(request, [&currentToken](JsonObject& respData) {
                respData["token"] = currentToken;
                respData["source"] = "nvs";
                respData["action"] = "set";
            });
        } else {
            sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                              ErrorCodes::INTERNAL_ERROR,
                              "Failed to set OTA token in NVS");
        }
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE,
                          "Invalid action. Use 'regenerate' or 'set'.",
                          "action");
    }
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
