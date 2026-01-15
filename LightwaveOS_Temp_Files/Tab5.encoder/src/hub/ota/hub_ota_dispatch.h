/**
 * @file hub_ota_dispatch.h
 * @brief OTA Dispatch - rolling update state machine
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include "hub_ota_repo.h"
#include "../net/hub_registry.h"

// OTA timeout constant with fallback
#ifndef LW_OTA_NODE_TIMEOUT_S
#define LW_OTA_NODE_TIMEOUT_S 180
#endif

typedef enum {
    OTA_DISPATCH_IDLE,
    OTA_DISPATCH_IN_PROGRESS,
    OTA_DISPATCH_COMPLETE,
    OTA_DISPATCH_ABORTED
} ota_dispatch_state_t;

class HubOtaDispatch {
public:
    HubOtaDispatch(HubRegistry* registry, HubOtaRepo* repo);
    
    bool begin();
    void tick(uint64_t now_ms);
    
    // Start rolling update for specific nodes
    bool startRollout(const char* track, const std::vector<uint8_t>& nodeIds);
    
    // Abort ongoing rollout
    void abort();
    
    // Get current dispatch state
    ota_dispatch_state_t getState() { return state_; }
    uint8_t getCurrentNode() { return currentNodeId_; }
    uint8_t getCompletedCount() { return completedCount_; }
    uint8_t getTotalCount() { return (uint8_t)nodeIds_.size(); }
    
    // Called when node reports OTA status
    void onNodeOtaStatus(uint8_t nodeId, const char* state, uint8_t pct, const char* error);
    
private:
    HubRegistry* registry_;
    HubOtaRepo* repo_;
    
    ota_dispatch_state_t state_;
    std::vector<uint8_t> nodeIds_;
    uint8_t currentNodeId_;
    uint8_t completedCount_;
    
    ota_release_t release_;
    uint64_t updateStartTime_ms_;
    
    bool sendUpdateToNode(uint8_t nodeId);
    void processNextNode(uint64_t now_ms);
};
