/**
 * @file node_ota_client.cpp
 * @brief Node OTA Client Implementation with SHA256 Verification
 */

#include "node_ota_client.h"
#include "../../common/util/logging.h"
#include "mbedtls/sha256.h"

#ifndef LW_LOG_TAG
#define LW_LOG_TAG "NodeOtaClient"
#endif

NodeOtaClient::NodeOtaClient() : state_(OTA_STATE_IDLE), progress_(0), onStatusChange(nullptr) {
    error_[0] = '\0';
}

bool NodeOtaClient::startUpdate(const char* url, const char* sha256, size_t expectedSize) {
    if (state_ != OTA_STATE_IDLE) {
        reportError("Update already in progress");
        return false;
    }
    
    url_ = url;
    expectedSha256_ = sha256;
    expectedSize_ = expectedSize;
    progress_ = 0;
    error_[0] = '\0';
    
    LW_LOGI("Starting OTA: url=%s size=%zu sha256=%s", url, expectedSize, sha256);
    
    state_ = OTA_STATE_DOWNLOADING;
    if (onStatusChange) onStatusChange("DOWNLOADING", 0, nullptr);
    
    return downloadAndVerify();
}

bool NodeOtaClient::downloadAndVerify() {
    HTTPClient http;
    http.begin(url_);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        reportError("HTTP GET failed");
        http.end();
        return false;
    }
    
    size_t contentLength = http.getSize();
    if (contentLength != expectedSize_) {
        reportError("Size mismatch");
        http.end();
        return false;
    }
    
    // Initialize SHA256 context
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);  // 0 = SHA256 (not SHA224)
    
    // Begin OTA update
    if (!Update.begin(expectedSize_)) {
        reportError("Update.begin() failed");
        mbedtls_sha256_free(&sha_ctx);
        http.end();
        return false;
    }
    
    state_ = OTA_STATE_DOWNLOADING;
    
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    size_t bytesWritten = 0;
    
    while (http.connected() && bytesWritten < contentLength) {
        size_t available = stream->available();
        if (available) {
            size_t toRead = min(available, sizeof(buf));
            size_t bytesRead = stream->readBytes(buf, toRead);
            
            if (bytesRead > 0) {
                // Update SHA256 hash
                mbedtls_sha256_update(&sha_ctx, buf, bytesRead);
                
                // Write to flash
                if (Update.write(buf, bytesRead) != bytesRead) {
                    reportError("Flash write failed");
                    Update.abort();
                    mbedtls_sha256_free(&sha_ctx);
                    http.end();
                    return false;
                }
                
                bytesWritten += bytesRead;
                progress_ = (uint8_t)((bytesWritten * 100) / contentLength);
                
                if (bytesWritten % 32768 == 0) {  // Log every 32KB
                    LW_LOGI("OTA progress: %u%% (%zu/%zu bytes)", progress_, bytesWritten, contentLength);
                }
            }
        }
        delay(1);  // Yield to watchdog
    }
    
    http.end();
    
    // Finalize SHA256
    state_ = OTA_STATE_VERIFYING;
    if (onStatusChange) onStatusChange("VERIFYING", 100, nullptr);
    
    uint8_t hash[32];
    mbedtls_sha256_finish(&sha_ctx, hash);
    mbedtls_sha256_free(&sha_ctx);
    
    // Convert hash to hex string
    char computedSha[65];
    for (int i = 0; i < 32; i++) {
        sprintf(computedSha + (2 * i), "%02x", hash[i]);
    }
    computedSha[64] = '\0';
    
    LW_LOGI("SHA256 computed: %s", computedSha);
    LW_LOGI("SHA256 expected: %s", expectedSha256_.c_str());
    
    // Verify SHA256
    if (expectedSha256_ != String(computedSha)) {
        reportError("SHA256 mismatch");
        Update.abort();
        return false;
    }
    
    LW_LOGI("SHA256 verified successfully");
    
    // Finalize update
    state_ = OTA_STATE_APPLYING;
    if (onStatusChange) onStatusChange("APPLYING", 100, nullptr);
    
    if (!Update.end(true)) {
        reportError("Update.end() failed");
        return false;
    }
    
    LW_LOGI("OTA update complete, rebooting...");
    state_ = OTA_STATE_REBOOTING;
    if (onStatusChange) onStatusChange("REBOOTING", 100, nullptr);
    
    delay(1000);
    ESP.restart();
    
    return true;
}

void NodeOtaClient::tick() {
    // State machine handled in downloadAndVerify() for now
    // Future: async chunked downloads
}

void NodeOtaClient::reportError(const char* msg) {
    strlcpy(error_, msg, sizeof(error_));
    state_ = OTA_STATE_ERROR;
    if (onStatusChange) onStatusChange("ERROR", progress_, msg);
    LW_LOGE("OTA error: %s", msg);
}
