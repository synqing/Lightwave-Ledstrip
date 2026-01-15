/**
 * @file hub_ota_dispatch.cpp
 * @brief OTA Dispatch Implementation
 */

#include "hub_ota_dispatch.h"
#include "../../common/util/logging.h"

#define LW_LOG_TAG "HubOtaDispatch"

// Forward declaration - will be set by HubMain
void (*g_sendOtaUpdateCallback)(uint8_t nodeId, const char* version, const char* url, const char* sha256) = nullptr;

HubOtaDispatch::HubOtaDispatch(HubRegistry* registry, HubOtaRepo* repo)
    : registry_(registry), repo_(repo), state_(OTA_DISPATCH_IDLE),
      currentNodeId_(0), completedCount_(0), updateStartTime_ms_(0) {}

bool HubOtaDispatch::begin() {
    LW_LOGI("OTA dispatcher initialized");
    return true;
}

void HubOtaDispatch::tick(uint64_t now_ms) {
    if (state_ != OTA_DISPATCH_IN_PROGRESS) return;
    
    // Check timeout
    if (now_ms - updateStartTime_ms_ > LW_OTA_NODE_TIMEOUT_S * 1000) {
        LW_LOGE("OTA timeout for node %u (elapsed %llu ms)",
                currentNodeId_, (unsigned long long)(now_ms - updateStartTime_ms_));
        abort();
        return;
    }
    
    // Check if current node is READY (update complete)
    node_entry_t* node = registry_->getNode(currentNodeId_);
    if (node && node->state == NODE_STATE_READY) {
        LW_LOGI("Node %u OTA complete, moving to next", currentNodeId_);
        completedCount_++;
        processNextNode(now_ms);
    }
}

bool HubOtaDispatch::startRollout(const char* track, const std::vector<uint8_t>& nodeIds) {
    if (state_ == OTA_DISPATCH_IN_PROGRESS) {
        LW_LOGW("Rollout already in progress");
        return false;
    }
    
    // Get release from manifest
    if (!repo_->getReleaseForTrack("k1", track, &release_)) {
        LW_LOGE("Failed to get release for track: %s", track);
        return false;
    }
    
    // Validate binary exists
    if (!repo_->validateBinaryPath(release_.url)) {
        LW_LOGE("Binary not found: %s", release_.url);
        return false;
    }
    
    nodeIds_ = nodeIds;
    completedCount_ = 0;
    state_ = OTA_DISPATCH_IN_PROGRESS;
    
    LW_LOGI("Starting rollout: track=%s version=%s nodes=%zu",
            track, release_.version, nodeIds_.size());
    
    processNextNode(millis());
    return true;
}

void HubOtaDispatch::abort() {
    if (state_ == OTA_DISPATCH_IN_PROGRESS) {
        LW_LOGW("Aborting rollout at node %u/%zu", completedCount_, nodeIds_.size());
    }
    state_ = OTA_DISPATCH_ABORTED;
    nodeIds_.clear();
    currentNodeId_ = 0;
}

void HubOtaDispatch::onNodeOtaStatus(uint8_t nodeId, const char* state, uint8_t pct, const char* error) {
    if (nodeId != currentNodeId_) {
        LW_LOGD("OTA status from non-current node %u (current=%u)", nodeId, currentNodeId_);
        return;
    }
    
    // Update registry OTA state
    registry_->setOtaState(nodeId, state, pct, release_.version, error);
    
    if (strcmp(state, "error") == 0) {
        LW_LOGE("Node %u OTA failed: %s", nodeId, error);
        abort();
    }
}

bool HubOtaDispatch::sendUpdateToNode(uint8_t nodeId) {
    if (!g_sendOtaUpdateCallback) {
        LW_LOGE("OTA callback not set");
        return false;
    }
    
    LW_LOGI("Sending OTA update to node %u: version=%s url=%s",
            nodeId, release_.version, release_.url);
    
    g_sendOtaUpdateCallback(nodeId, release_.version, release_.url, release_.sha256);
    return true;
}

void HubOtaDispatch::processNextNode(uint64_t now_ms) {
    if (completedCount_ >= nodeIds_.size()) {
        LW_LOGI("Rollout complete! Updated %zu nodes", nodeIds_.size());
        state_ = OTA_DISPATCH_COMPLETE;
        nodeIds_.clear();
        return;
    }
    
    currentNodeId_ = nodeIds_[completedCount_];
    updateStartTime_ms_ = now_ms;
    
    sendUpdateToNode(currentNodeId_);
}
