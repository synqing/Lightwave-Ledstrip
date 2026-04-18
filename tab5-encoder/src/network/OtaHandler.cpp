// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file OtaHandler.cpp
 * @brief OTA firmware update handlers implementation
 */

#include "OtaHandler.h"
#include "../config/network_config.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
#include <cstring>

// Static member initialization
bool OtaHandler::s_updateStarted = false;
bool OtaHandler::s_updateError = false;
String OtaHandler::s_updateErrorMessage = "";
size_t OtaHandler::s_updateProgress = 0;
size_t OtaHandler::s_updateTotal = 0;

// Firmware version constants
static constexpr const char* FIRMWARE_VERSION = "1.0.0";
static constexpr const char* BOARD_NAME = "M5Stack-Tab5-ESP32-P4";

// ============================================================================
// Wave 2 — session watchdog + SHA-256 integrity state
// ============================================================================

/// Time-since-last-data watchdog window (ms). A session with no chunk arrival
/// for longer than this is considered wedged and is aborted by OtaHandler::loop().
static constexpr uint32_t OTA_SESSION_TIMEOUT_MS = 30000;

/// Wall-clock of the LAST received chunk (or session start). Reset on every
/// byte through handleUpload so the watchdog tracks "time since last data",
/// not "time since session start" — a legitimately-slow uploader is allowed.
static uint32_t s_updateStartedAtMs = 0;

/// SHA-256 streaming context. ~104 B in .bss, re-used across sessions.
/// NEVER heap-allocated. sha256BeginSession() initialises; sha256ReleaseSession()
/// frees and MUST be called on every terminal path.
static mbedtls_sha256_context s_sha256Ctx;
static bool s_sha256Active = false;

/// Expected SHA-256 from the X-OTA-SHA256 header, lower-case hex, 64 chars.
/// Empty when no session is active.
static String s_expectedSha256;

// ============================================================================
// mbedtls version guard — 3.x drops the _ret suffix.
// mbedtls 3.0 bumped MBEDTLS_VERSION_NUMBER to 0x03000000 and deprecated the
// _ret variants (which return int instead of void). Older ESP-IDF / Arduino-core
// ships mbedtls 2.x where the non-suffixed forms are void-returning. Use the
// int-returning forms under either name so the error check below is meaningful.
// ============================================================================
#if defined(MBEDTLS_VERSION_NUMBER) && (MBEDTLS_VERSION_NUMBER >= 0x03000000)
    #define OTA_SHA256_STARTS(ctx)          mbedtls_sha256_starts((ctx), 0)
    #define OTA_SHA256_UPDATE(ctx, in, sz)  mbedtls_sha256_update((ctx), (in), (sz))
    #define OTA_SHA256_FINISH(ctx, out)     mbedtls_sha256_finish((ctx), (out))
#else
    #define OTA_SHA256_STARTS(ctx)          mbedtls_sha256_starts_ret((ctx), 0)
    #define OTA_SHA256_UPDATE(ctx, in, sz)  mbedtls_sha256_update_ret((ctx), (in), (sz))
    #define OTA_SHA256_FINISH(ctx, out)     mbedtls_sha256_finish_ret((ctx), (out))
#endif

// ============================================================================
// SHA-256 integrity helpers — ported verbatim (with mbedtls-version shim) from
// firmware-v3/src/network/webserver/ws/WsOtaCommands.cpp:164-241. Same contract:
// heap-free, single file-scope ctx, idempotent release on every terminal path.
// ============================================================================

/// Validate a 64-char lower-case hex string and copy to outBuf.
/// Returns true if len==64 and all chars are [0-9a-fA-F]. Lower-cases as it copies.
static bool normaliseHexHash(const char* in, size_t expectedLen, char* outBuf, size_t outBufSize) {
    if (!in || outBufSize < expectedLen + 1) return false;
    size_t len = strlen(in);
    if (len != expectedLen) return false;
    for (size_t i = 0; i < len; i++) {
        char c = in[i];
        if (c >= '0' && c <= '9') { outBuf[i] = c; continue; }
        if (c >= 'a' && c <= 'f') { outBuf[i] = c; continue; }
        if (c >= 'A' && c <= 'F') { outBuf[i] = static_cast<char>(c + ('a' - 'A')); continue; }
        return false;
    }
    outBuf[expectedLen] = '\0';
    return true;
}

/// Initialise the session SHA-256 context. Safe to re-init (frees old).
/// Call once at upload start AFTER the header has been validated.
static bool sha256BeginSession() {
    if (s_sha256Active) {
        mbedtls_sha256_free(&s_sha256Ctx);
        s_sha256Active = false;
    }
    mbedtls_sha256_init(&s_sha256Ctx);
    if (OTA_SHA256_STARTS(&s_sha256Ctx) != 0) {
        mbedtls_sha256_free(&s_sha256Ctx);
        return false;
    }
    s_sha256Active = true;
    return true;
}

/// Feed accepted chunk bytes into the running SHA-256 calculation.
/// Called from the streaming path — must not allocate.
static bool sha256FeedChunk(const unsigned char* data, size_t len) {
    if (!s_sha256Active || !data || len == 0) return s_sha256Active;
    return OTA_SHA256_UPDATE(&s_sha256Ctx, data, len) == 0;
}

/// Finalise the SHA-256 and write the lower-case hex digest to out[65].
/// Frees the ctx regardless of outcome.
static bool sha256FinaliseHex(char* outHex65) {
    if (!s_sha256Active || !outHex65) return false;
    unsigned char digest[32];
    int rc = OTA_SHA256_FINISH(&s_sha256Ctx, digest);
    mbedtls_sha256_free(&s_sha256Ctx);
    s_sha256Active = false;
    if (rc != 0) return false;
    static const char* kHex = "0123456789abcdef";
    for (size_t i = 0; i < 32; i++) {
        outHex65[i * 2]     = kHex[(digest[i] >> 4) & 0x0F];
        outHex65[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    outHex65[64] = '\0';
    return true;
}

/// Release SHA-256 resources on any abort / failure path. Idempotent.
/// MUST be called on EVERY terminal path: success, mismatch, abort, timeout,
/// chunk-feed failure, token failure, size failure, Update.begin failure.
static void sha256ReleaseSession() {
    if (s_sha256Active) {
        mbedtls_sha256_free(&s_sha256Ctx);
        s_sha256Active = false;
    }
}

/// Constant-time 64-byte compare (both buffers already normalised lower-case).
/// Prevents timing side-channel on integrity comparison.
static bool constTimeHexEquals(const char* a, const char* b, size_t len) {
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i]);
    }
    return (diff == 0);
}

void OtaHandler::handleVersion(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["version"] = FIRMWARE_VERSION;
        data["board"] = BOARD_NAME;
        data["sdk"] = ESP.getSdkVersion();
        data["sketchSize"] = ESP.getSketchSize();
        data["freeSketch"] = ESP.getFreeSketchSpace();
        data["flashSize"] = ESP.getFlashChipSize();
        data["buildDate"] = __DATE__;
        data["buildTime"] = __TIME__;
        data["chipModel"] = ESP.getChipModel();
        data["chipRevision"] = ESP.getChipRevision();
        data["cpuFreq"] = ESP.getCpuFreqMHz();

        // Calculate OTA capacity
        size_t maxOtaSize = ESP.getFreeSketchSpace();
        data["maxOtaSize"] = maxOtaSize;
        data["otaAvailable"] = maxOtaSize > 0;
    });
}

bool OtaHandler::checkOTAToken(AsyncWebServerRequest* request) {
    const char* expectedToken = OTA_UPDATE_TOKEN;

    // Check for X-OTA-Token header
    if (!request->hasHeader("X-OTA-Token")) {
        Serial.printf("[OTA] Request missing X-OTA-Token header from %s\n",
                      request->client()->remoteIP().toString().c_str());
        sendErrorResponse(request, 401, "Missing X-OTA-Token header");
        return false;
    }

    // Validate token
    String providedToken = request->header("X-OTA-Token");
    if (providedToken != expectedToken) {
        Serial.printf("[OTA] Request with invalid token from %s\n",
                      request->client()->remoteIP().toString().c_str());
        sendErrorResponse(request, 401, "Invalid OTA token");
        return false;
    }

    return true;
}

void OtaHandler::handleV1Update(AsyncWebServerRequest* request) {
    // This is called after upload completes
    if (s_updateError) {
        Serial.printf("[OTA] Update failed: %s\n", s_updateErrorMessage.c_str());
        sendErrorResponse(request, 500, s_updateErrorMessage.c_str());
        // Reset state for next attempt
        s_updateError = false;
        s_updateErrorMessage = "";
        s_updateStarted = false;
        s_updateStartedAtMs = 0;
        sha256ReleaseSession();
        s_expectedSha256 = "";
        return;
    }

    if (!s_updateStarted) {
        // No upload was processed
        sendErrorResponse(request, 400, "No firmware data received");
        return;
    }

    // Success - device will reboot
    Serial.println("[OTA] Update successful, rebooting...");

    sendSuccessResponse(request, [](JsonObject& data) {
        data["message"] = "Firmware update successful. Device is rebooting.";
        data["rebooting"] = true;
    });

    // Reset state
    s_updateStarted = false;
    s_updateStartedAtMs = 0;
    s_updateProgress = 0;
    s_updateTotal = 0;
    sha256ReleaseSession();
    s_expectedSha256 = "";

    // Delay to allow response to be sent, then reboot
    delay(500);
    ESP.restart();
}

void OtaHandler::handleLegacyUpdate(AsyncWebServerRequest* request) {
    // This is called after upload completes
    if (s_updateError) {
        Serial.printf("[OTA] Update failed: %s\n", s_updateErrorMessage.c_str());
        request->send(500, "text/plain", "Update failed: " + s_updateErrorMessage);
        // Reset state for next attempt
        s_updateError = false;
        s_updateErrorMessage = "";
        s_updateStarted = false;
        s_updateStartedAtMs = 0;
        sha256ReleaseSession();
        s_expectedSha256 = "";
        return;
    }

    if (!s_updateStarted) {
        // No upload was processed
        request->send(400, "text/plain", "No firmware data received");
        return;
    }

    // Success - device will reboot
    Serial.println("[OTA] Update successful, rebooting...");
    request->send(200, "text/plain", "Update successful. Rebooting...");

    // Reset state
    s_updateStarted = false;
    s_updateStartedAtMs = 0;
    s_updateProgress = 0;
    s_updateTotal = 0;
    sha256ReleaseSession();
    s_expectedSha256 = "";

    // Delay to allow response to be sent, then reboot
    delay(500);
    ESP.restart();
}

void OtaHandler::handleUpload(AsyncWebServerRequest* request,
                               const String& filename,
                               size_t index,
                               uint8_t* data,
                               size_t len,
                               bool final) {
    // On first chunk (index == 0), initialise update
    if (index == 0) {
        Serial.printf("[OTA] Upload starting: %s\n", filename.c_str());

        // Validate OTA token before starting update
        if (!checkOTAToken(request)) {
            s_updateError = true;
            s_updateErrorMessage = "Unauthorised";
            return;
        }

        // P0-03: Integrity hash is MANDATORY. Hard-reject if the X-OTA-SHA256
        // header is absent or malformed. Policy matches the K1 WS OTA contract
        // (docs/protocol/k1-rest-contract.yaml) so the same uploader can talk
        // to either device.
        char normalised[65] = {0};
        if (!request->hasHeader("X-OTA-SHA256")) {
            s_updateError = true;
            s_updateErrorMessage = "Integrity hash required: supply X-OTA-SHA256 (64 hex)";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            return;
        }
        {
            String supplied = request->header("X-OTA-SHA256");
            if (!normaliseHexHash(supplied.c_str(), 64, normalised, sizeof(normalised))) {
                s_updateError = true;
                s_updateErrorMessage = "Integrity hash required: supply X-OTA-SHA256 (64 hex)";
                Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
                return;
            }
        }

        // Reset state
        s_updateError = false;
        s_updateErrorMessage = "";
        s_updateStarted = true;
        s_updateStartedAtMs = millis();  // P0-04: arm the session watchdog
        s_updateProgress = 0;
        s_expectedSha256 = normalised;    // lower-case, 64 hex chars

        // Get content length
        s_updateTotal = request->contentLength();
        Serial.printf("[OTA] Firmware size: %u bytes\n", s_updateTotal);

        // Check available space
        size_t freeSpace = ESP.getFreeSketchSpace();
        if (s_updateTotal > freeSpace) {
            s_updateError = true;
            s_updateErrorMessage = "Firmware too large. Available: " + String(freeSpace) + " bytes";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            sha256ReleaseSession();
            s_expectedSha256 = "";
            return;
        }

        // Initialise the running SHA-256 BEFORE Update.begin so any failure
        // here leaves both subsystems clean.
        if (!sha256BeginSession()) {
            s_updateError = true;
            s_updateErrorMessage = "Failed to initialise SHA-256 context";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            Serial.println("[OTA] Failed to initialise SHA-256 context");
            s_expectedSha256 = "";
            return;
        }

        // Begin update
        if (!Update.begin(s_updateTotal, U_FLASH)) {
            s_updateError = true;
            s_updateErrorMessage = "Update.begin() failed: " + String(Update.errorString());
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            sha256ReleaseSession();
            s_expectedSha256 = "";
            return;
        }

        Serial.printf("[OTA] Update started, expecting %u bytes (SHA-256 armed)\n", s_updateTotal);
    }

    // Skip processing if we've already encountered an error
    if (s_updateError) {
        return;
    }

    // Write chunk to flash
    if (len > 0) {
        size_t written = Update.write(data, len);
        if (written != len) {
            s_updateError = true;
            s_updateErrorMessage = "Flash write failed at offset " + String(index);
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            Update.abort();
            sha256ReleaseSession();
            s_expectedSha256 = "";
            return;
        }

        // P0-03: feed bytes into SHA-256 AFTER Update.write succeeded — never
        // hash bytes that were rejected by the flash writer.
        if (s_sha256Active && !sha256FeedChunk(data, len)) {
            s_updateError = true;
            s_updateErrorMessage = "SHA-256 streaming failure";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            Serial.println("[OTA] SHA-256 streaming failure");
            Update.abort();
            sha256ReleaseSession();
            s_expectedSha256 = "";
            return;
        }

        // P0-04: reset the watchdog on every accepted chunk — timeout is
        // "time since last data", not "time since session start".
        s_updateStartedAtMs = millis();

        s_updateProgress += len;

        // Log progress every ~10%
        if (s_updateTotal > 0) {
            static uint8_t lastPercent = 0;
            uint8_t percent = (s_updateProgress * 100) / s_updateTotal;
            if (percent / 10 > lastPercent / 10) {
                Serial.printf("[OTA] Progress: %u%%\n", percent);
                lastPercent = percent;
            }
        }
    }

    // On final chunk, complete update
    if (final) {
        Serial.println("[OTA] Upload complete, finalising...");

        // P0-03: finalise SHA BEFORE Update.end(true). A mismatched image must
        // NEVER be allowed to swap partitions, so we call Update.abort() and
        // bail out before the commit.
        char computedHex[65] = {0};
        if (!sha256FinaliseHex(computedHex)) {
            s_updateError = true;
            s_updateErrorMessage = "SHA-256 finalise failure";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            Update.abort();
            sha256ReleaseSession();
            s_expectedSha256 = "";
            return;
        }

        if (!constTimeHexEquals(computedHex, s_expectedSha256.c_str(), 64)) {
            s_updateError = true;
            s_updateErrorMessage = "SHA-256 mismatch — image rejected";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            Serial.println("[OTA] SHA-256 mismatch — image rejected");
            Update.abort();
            sha256ReleaseSession();  // already released by finalise, idempotent
            s_expectedSha256 = "";
            return;
        }

        if (!Update.end(true)) {
            s_updateError = true;
            s_updateErrorMessage = "Update.end() failed: " + String(Update.errorString());
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            sha256ReleaseSession();
            s_expectedSha256 = "";
            return;
        }

        if (!Update.isFinished()) {
            s_updateError = true;
            s_updateErrorMessage = "Update not finished properly";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            sha256ReleaseSession();
            s_expectedSha256 = "";
            return;
        }

        // Success path — context has already been freed by sha256FinaliseHex.
        s_expectedSha256 = "";
        Serial.println("[OTA] Upload finalised successfully (SHA-256 verified)");
    }
}

void OtaHandler::loop() {
    if (!s_updateStarted) return;
    const uint32_t now = millis();
    const uint32_t elapsed = now - s_updateStartedAtMs;
    if (elapsed <= OTA_SESSION_TIMEOUT_MS) return;

    // P0-04: session has stalled with no fresh data for >30 s. Assume the
    // client has dropped and the session is wedged. Abort cleanly so the
    // next upload attempt can acquire the Update library.
    Update.abort();
    s_updateError = true;
    s_updateErrorMessage = "Session timed out";
    sha256ReleaseSession();
    s_expectedSha256 = "";
    s_updateStarted = false;
    s_updateStartedAtMs = 0;
    s_updateProgress = 0;
    s_updateTotal = 0;
    Serial.println("[OTA] Session timed out after 30 s — aborted");
}

uint8_t OtaHandler::getProgress() {
    if (!s_updateStarted || s_updateTotal == 0) {
        return 0;
    }
    return (s_updateProgress * 100) / s_updateTotal;
}

bool OtaHandler::isUpdating() {
    return s_updateStarted && !s_updateError;
}

void OtaHandler::sendErrorResponse(AsyncWebServerRequest* request,
                                    int code,
                                    const char* message) {
    JsonDocument doc;
    doc["success"] = false;
    doc["error"] = message;
    doc["code"] = code;
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    request->send(code, "application/json", output);
}

void OtaHandler::sendSuccessResponse(AsyncWebServerRequest* request,
                                      std::function<void(JsonObject&)> fillData) {
    JsonDocument doc;
    doc["success"] = true;
    JsonObject data = doc["data"].to<JsonObject>();
    fillData(data);
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

