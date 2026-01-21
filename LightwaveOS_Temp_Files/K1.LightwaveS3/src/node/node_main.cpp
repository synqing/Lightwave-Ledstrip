/**
 * Node Main Coordinator Implementation
 */

#include "node_main.h"
#include "../common/util/logging.h"
#include "../common/proto/udp_packets.h"
#include "../common/clock/monotonic.h"
#include <esp_wifi.h>

// g_nodeMain is defined in node_integration.h (included by main.cpp)

bool NodeMain::init(const char* hub_ssid, const char* hub_password) {
    LW_LOGI("=== Node Initialization ===");
    
    // Initialize subsystems
    timesync_.init();
    tsUdp_.setTimesync(timesync_.getState());  // Wire up after timesync is initialized
    scheduler_.init();

    // Wire WS control-plane into the scheduler so hub broadcasts can be applied.
    ws_.setTimeSync(&timesync_);
    ws_.setScheduler(&scheduler_);
    
    // Initialize renderer with orchestrator (if provided)
    if (orchestrator_) {
        renderer_.init(orchestrator_);
        LW_LOGI("Renderer wired to v2 Actor system");
    } else {
        LW_LOGW("No orchestrator provided - LED rendering will not work!");
    }
    
    fallback_.init();
    ota_.init();
    
    // Start WiFi
    if (!wifi_.init(hub_ssid, hub_password)) {
        LW_LOGE("Failed to init WiFi");
        return false;
    }
    
    // Setup WS callbacks
    ws_.onWelcome = onWelcome;
    ws_.onTsPong = onTsPong;
    
    // Start WS client (will connect when WiFi ready)
    if (!ws_.init(LW_HUB_IP)) {
        LW_LOGE("Failed to init WS");
        return false;
    }
    
    // Start UDP RX (will work when WiFi ready)
    if (!udp_.init()) {
        LW_LOGE("Failed to init UDP");
        return false;
    }
    
    // Wire WS to UDP RX for fanout disarm on disconnect
    ws_.setUdpRx(&udp_);
    
    // Wire WS to OTA client for remote updates
    ws_.setOtaClient(&ota_);
    
    // Wire OTA status callback
    ota_.onStatusChange = onOtaStatus;
    
    initialized_ = true;
    systemState_ = NODE_SYSTEM_STATE_CONNECTING;
    
    LW_LOGI("=== Node Ready ===");
    LW_LOGI("  Hub: %s", hub_ssid);
    LW_LOGI("  Target: ws://%s:%d/ws", LW_HUB_IP, LW_CTRL_HTTP_PORT);
    
    return true;
}

void NodeMain::loop() {
    if (!initialized_) return;
    
    // Update all subsystems
    wifi_.loop();
    ws_.loop();
    tsUdp_.loop();  // UDP time-sync (replaces WS ts_ping/pong)
    udp_.loop();
    timesync_.tick();
    ota_.tick();

    // Track time-sync health as the UDP liveness signal in hub-controlled mode.
    // This stays valid even when show-UDP is intentionally silent (e.g. audio inactive).
    {
        const lw_timesync_t* ts = timesync_.getState();
        if (ts && ts->last_pong_us != 0) {
            const uint64_t now_us = lw_monotonic_us();
            if (now_us >= ts->last_pong_us) {
                const uint32_t age_ms = (uint32_t)((now_us - ts->last_pong_us) / 1000ULL);
                const uint32_t now_ms = millis();
                lastUdp_ms_ = (age_ms <= now_ms) ? (uint64_t)(now_ms - age_ms) : 0;
            }
        }
    }
    
    // Update fallback policy
    fallback_.update(lastUdp_ms_, udp_.getLossPercent(), timesync_.getDriftUs());
    
    // Update system state
    updateSystemState();
}

void NodeMain::renderFrameBoundary() {
    if (!initialized_) return;
    
    // Apply due commands (this is the ONLY place commands are applied)
    renderer_.applyCommands(&scheduler_);
}

void NodeMain::updateSystemState() {
    node_system_state_t oldState = systemState_;
    
    if (!wifi_.isConnected()) {
        systemState_ = NODE_SYSTEM_STATE_OFFLINE;
    } else if (!ws_.isAuthenticated()) {
        systemState_ = NODE_SYSTEM_STATE_CONNECTING;
    } else if (checkReadyGate()) {
        if (fallback_.isDegraded()) {
            systemState_ = NODE_SYSTEM_STATE_DEGRADED;
        } else {
            systemState_ = NODE_SYSTEM_STATE_READY;
        }
    } else {
        systemState_ = NODE_SYSTEM_STATE_CONNECTING;
    }
    
    if (fallback_.isActive()) {
        systemState_ = NODE_SYSTEM_STATE_FAILED;
    }
    
    // Log state changes
    if (oldState != systemState_) {
        const char* states[] = {"OFFLINE", "CONNECTING", "READY", "DEGRADED", "FAILED"};
        LW_LOGI("System state: %s -> %s", states[oldState], states[systemState_]);
    }
}

bool NodeMain::checkReadyGate() {
    if (!wifi_.isConnected()) return false;
    if (!ws_.isAuthenticated()) return false;
    return true;
}

void NodeMain::onWelcome(uint8_t nodeId, const char* token) {
    // Set token hash for UDP validation
    uint32_t tokenHash = lw_token_hash32(token);
    g_nodeMain.udp_.setTokenHash(tokenHash);
    
    // Initialize UDP time-sync
    if (!g_nodeMain.tsUdp_.init(LW_HUB_IP, tokenHash)) {
        LW_LOGE("Failed to init UDP time-sync");
    }
    
    LW_LOGI("WELCOME received: nodeId=%d, tokenHash=0x%08X, UDP TS started", nodeId, tokenHash);
}

void NodeMain::onTsPong(uint32_t seq, uint64_t t1_us, uint64_t t2_us, uint64_t t3_us) {
    bool was_locked = g_nodeMain.timesync_.isLocked();
    
    g_nodeMain.timesync_.processPong(seq, t1_us, t2_us, t3_us);
    
    bool now_locked = g_nodeMain.timesync_.isLocked();
    if (!was_locked && now_locked) {
        LW_LOGI("Time sync LOCKED (WS)");
    } else if (was_locked && !now_locked) {
        LW_LOGW("Time sync UNLOCKED (WS)");
    }
}

void NodeMain::onOtaStatus(const char* state, uint8_t progress, const char* error) {
    // Forward OTA status to Hub via WebSocket
    g_nodeMain.ws_.sendOtaStatus(state, progress, error);
}
