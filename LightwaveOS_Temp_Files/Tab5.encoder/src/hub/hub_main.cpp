/**
 * Hub Main Coordinator Implementation
 */

#include "hub_main.h"
#include "../common/util/logging.h"
#include <LittleFS.h>

// Forward declarations for OTA callback
extern void (*g_sendOtaUpdateCallback)(uint8_t nodeId, const char* version, const char* url, const char* sha256);
static HubHttpWs* g_httpWs = nullptr;

HubMain* HubMain::instance_ = nullptr;

bool HubMain::init(const char* ssid, const char* password) {
    LW_LOGI("=== Hub Initialization ===");
    
    // Initialize clock
    hub_clock_init(&clock_);
    
    // Mount LittleFS
    if (!LittleFS.begin(true)) {
        LW_LOGW("LittleFS mount failed (OTA will not be available)");
    } else {
        LW_LOGI("LittleFS mounted successfully");
        
        // Initialize OTA repository
        if (otaRepo_.begin(&LittleFS)) {
            LW_LOGI("OTA repository initialized");
        } else {
            LW_LOGW("OTA repository initialization failed (manifest not found?)");
        }
        
        // Initialize OTA dispatcher
        if (otaDispatch_.begin()) {
            LW_LOGI("OTA dispatcher initialized");
        }
    }
    
    // Start SoftAP
    if (!softap_.init(ssid, password, LW_HUB_IP)) {
        LW_LOGE("Failed to start SoftAP");
        return false;
    }
    
    // Start HTTP + WebSocket server
    if (!httpWs_.init(LW_CTRL_HTTP_PORT)) {
        LW_LOGE("Failed to start HTTP/WS server");
        return false;
    }

    // Link HubState to HTTP/WS layer (snapshots + broadcasts)
    httpWs_.setState(&HubState::getInstance());
    
    // Wire OTA to HTTP/WS server
    httpWs_.setOta(&otaRepo_, &otaDispatch_);
    g_httpWs = &httpWs_;
    g_sendOtaUpdateCallback = [](uint8_t nodeId, const char* version, const char* url, const char* sha256) {
        if (g_httpWs) {
            g_httpWs->sendOtaUpdate(nodeId, version, url, sha256);
        }
    };
    
    // Start UDP fanout
    if (!udpFanout_.init()) {
        LW_LOGE("Failed to start UDP fanout");
        return false;
    }
    udpFanout_.setState(&HubState::getInstance());
    
    // Start UDP time-sync listener
    if (!tsUdp_.init()) {
        LW_LOGE("Failed to start UDP time-sync");
        return false;
    }
    
    instance_ = this;
    
    initialized_ = true;
    LW_LOGI("=== Hub Ready ===");
    LW_LOGI("  SSID: %s", ssid);
    LW_LOGI("  IP: %s", softap_.getIP().c_str());
    LW_LOGI("  WS: ws://%s/ws", softap_.getIP().c_str());
    LW_LOGI("  UDP: %s:%d", softap_.getIP().c_str(), LW_UDP_PORT);
    
    return true;
}

void HubMain::loop() {
    if (!initialized_) return;
    
    // WS cleanup
    httpWs_.loop();
    
    // UDP time-sync listener
    tsUdp_.loop();
    
    // Registry maintenance (timeouts, cleanup)
    registry_.tick(millis());
    
    // OTA dispatcher tick (rolling update state machine)
    otaDispatch_.tick(millis());

    // Phase 1: 50ms batching for WebSocket broadcasts (encoder jitter storm protection)
    const uint32_t now_ms = millis();
    if (now_ms - lastBatch_ms_ >= 50) {
        lastBatch_ms_ = now_ms;

        HubState& state = HubState::getInstance();
        if (state.hasDirty()) {
            // Compute applyAt_us ONCE per batch window (identical for all nodes for this broadcast)
            const uint64_t applyAt_us = hub_clock_now_us(&clock_) + LW_APPLY_AHEAD_US;

            const HubState::GlobalDelta g = state.consumeGlobalDelta();
            if (g.dirtyMask != 0) {
                httpWs_.broadcastGlobalDelta(g.dirtyMask, g.values, applyAt_us);
                if (ctrlVerbose_) {
                    LW_LOGI("[HUB-CTRL] applyAt_us=%llu dirty=0x%04X effect=%u palette=%u bright=%u speed=%u hue=%u intensity=%u saturation=%u complexity=%u variation=%u",
                            (unsigned long long)applyAt_us,
                            (unsigned)g.dirtyMask,
                            (unsigned)g.values.effectId,
                            (unsigned)g.values.paletteId,
                            (unsigned)g.values.brightness,
                            (unsigned)g.values.speed,
                            (unsigned)g.values.hue,
                            (unsigned)g.values.intensity,
                            (unsigned)g.values.saturation,
                            (unsigned)g.values.complexity,
                            (unsigned)g.values.variation);
                }
            }

            HubState::ZoneDelta zoneDeltas[32];
            const uint8_t count = state.consumeZoneDeltas(zoneDeltas, (uint8_t)(sizeof(zoneDeltas) / sizeof(zoneDeltas[0])));
            for (uint8_t i = 0; i < count; ++i) {
                // Only broadcast to READY nodes; HubHttpWs will no-op if the node has no active client.
                if (registry_.isReady(zoneDeltas[i].nodeId)) {
                    httpWs_.sendZoneDelta(zoneDeltas[i].nodeId, zoneDeltas[i].zoneId, zoneDeltas[i].dirtyMask, zoneDeltas[i].values, applyAt_us);
                    if (ctrlVerbose_) {
                        LW_LOGI("[HUB-ZONES] applyAt_us=%llu node=%u zone=%u dirty=0x%02X effect=%u palette=%u bright=%u speed=%u blend=%u",
                                (unsigned long long)applyAt_us,
                                (unsigned)zoneDeltas[i].nodeId,
                                (unsigned)zoneDeltas[i].zoneId,
                                (unsigned)zoneDeltas[i].dirtyMask,
                                (unsigned)zoneDeltas[i].values.effectId,
                                (unsigned)zoneDeltas[i].values.paletteId,
                                (unsigned)zoneDeltas[i].values.brightness,
                                (unsigned)zoneDeltas[i].values.speed,
                                (unsigned)zoneDeltas[i].values.blendMode);
                    }
                }
            }
        }
    }
}

void HubMain::loopNoDisplay() {
    loop(); // Same as loop now, no display in HubMain
}

void HubMain::udpTick() {
    if (!initialized_) return;
    
    // Send UDP packets to all READY nodes
    udpFanout_.tick();
}

void HubMain::onRegistryEvent(uint8_t nodeId, uint8_t eventType, const char* message) {
    // Event logging (future: integrate with LVGL dashboard)
    char buf[64];
    if (nodeId > 0) {
        snprintf(buf, sizeof(buf), "N%d: %s", nodeId, message);
    } else {
        strlcpy(buf, message, sizeof(buf));
    }
    LW_LOGI("[EVENT] %s", buf);
}
