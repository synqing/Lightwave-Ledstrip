/**
 * @file node_ota_client.h
 * @brief Node OTA Client - Downloads and verifies firmware updates
 */

#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>

typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_APPLYING,
    OTA_STATE_REBOOTING,
    OTA_STATE_ERROR
} node_ota_state_t;

class NodeOtaClient {
public:
    NodeOtaClient();
    
    // Initialize (called once at startup)
    void init() { /* No-op for now */ }
    
    // Start OTA update
    bool startUpdate(const char* url, const char* sha256, size_t expectedSize);
    
    // Tick (call periodically to process update)
    void tick();
    
    // Get current state and progress
    node_ota_state_t getState() const { return state_; }
    uint8_t getProgress() const { return progress_; }
    const char* getError() const { return error_; }
    
    // Status reporting callback (set by NodeMain)
    void (*onStatusChange)(const char* state, uint8_t progress, const char* error);
    
private:
    node_ota_state_t state_;
    uint8_t progress_;
    char error_[128];
    
    String url_;
    String expectedSha256_;
    size_t expectedSize_;
    
    bool downloadAndVerify();
    void reportError(const char* msg);
};
