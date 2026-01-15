/**
 * Hub Registry Implementation
 */

#include "hub_registry.h"
#include "../../common/util/logging.h"
#include "../../common/clock/monotonic.h"
#include "../../common/proto/udp_packets.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <cstdio>
#include <cstring>

// ===== Phase 3 assertion logging =====
#define P3_PASS(code, fmt, ...) LW_LOGI("[P3-PASS][%s] " fmt, code, ##__VA_ARGS__)
#define P3_FAIL(code, fmt, ...) LW_LOGE("[P3-FAIL][%s] " fmt, code, ##__VA_ARGS__)
#define P3_WARN(code, fmt, ...) LW_LOGW("[P3-WARN][%s] " fmt, code, ##__VA_ARGS__)

static inline bool p3_every_ms(uint32_t* last_ms, uint32_t period_ms) {
    uint32_t now = (uint32_t)millis();
    if (now - *last_ms >= period_ms) { *last_ms = now; return true; }
    return false;
}

static uint8_t getSoftApStationCount() {
    wifi_sta_list_t list;
    esp_err_t err = esp_wifi_ap_get_sta_list(&list);
    if (err == ESP_OK) {
        return (uint8_t)list.num;
    }
    return (uint8_t)WiFi.softAPgetStationNum();
}

HubRegistry::HubRegistry() : nextNodeId_(1), nextToken_(1000), eventCallback_(nullptr) {
    LW_LOGI("Hub registry initialized");
}

uint8_t HubRegistry::registerNode(const lw_msg_hello_t* hello, const char* ip) {
    // Check if node already registered by MAC
    for (auto& pair : nodes_) {
        if (strcmp(pair.second.mac, hello->mac) == 0) {
            pair.second.state = NODE_STATE_PENDING;
            strncpy(pair.second.ip, ip, sizeof(pair.second.ip) - 1);
            
            // Clear stale session token (fanout must not send until after WELCOME)
            pair.second.tokenHash = 0;
            pair.second.token[0] = '\0';
            
            // Reset OTA state on rejoin
            strlcpy(pair.second.ota_state, "idle", sizeof(pair.second.ota_state));
            pair.second.ota_pct = 0;
            pair.second.ota_version[0] = '\0';
            pair.second.ota_error[0] = '\0';
            
            P3_PASS("HRG_REJOIN",
                    "mac=%s nodeId=%u ip=%s state->PENDING tokenCleared=1",
                    hello->mac, pair.first, ip);
            
            if (eventCallback_) {
                char msg[100];
                snprintf(msg, sizeof(msg), "HELLO (rejoin) MAC=%s IP=%s", hello->mac, ip);
                eventCallback_(pair.first, EVENT_NODE_HELLO, msg);
            }
            
            return pair.first;
        }
    }
    
    // New node
    if (nextNodeId_ >= LW_MAX_NODES) {
        LW_LOGE("Max nodes (%d) reached, cannot register %s", LW_MAX_NODES, hello->mac);
        return 0;
    }
    
    uint8_t nodeId = nextNodeId_++;
    node_entry_t entry = {};
    entry.nodeId = nodeId;
    strncpy(entry.mac, hello->mac, sizeof(entry.mac) - 1);
    strncpy(entry.ip, ip, sizeof(entry.ip) - 1);
    strncpy(entry.fw, hello->fw, sizeof(entry.fw) - 1);
    entry.caps = hello->caps;
    entry.topo = hello->topo;
    entry.state = NODE_STATE_PENDING;
    entry.lastSeen_ms = millis();
    
    // Initialize OTA state
    strlcpy(entry.ota_state, "idle", sizeof(entry.ota_state));
    entry.ota_pct = 0;
    entry.ota_version[0] = '\0';
    entry.ota_error[0] = '\0';
    
    nodes_[nodeId] = entry;
    
    P3_PASS("HRG_NEW",
            "mac=%s nodeId=%u ip=%s fw=%s state=PENDING",
            entry.mac, nodeId, entry.ip, entry.fw);
    
    if (eventCallback_) {
        char msg[100];
        snprintf(msg, sizeof(msg), "HELLO (new) MAC=%s IP=%s FW=%s", entry.mac, entry.ip, entry.fw);
        eventCallback_(nodeId, EVENT_NODE_HELLO, msg);
    }
    
    return nodeId;
}

bool HubRegistry::sendWelcome(uint8_t nodeId, lw_msg_welcome_t* welcome) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) return false;
    
    node_entry_t& node = it->second;
    
    // Generate unique token
    generateToken(node.token, sizeof(node.token));
    node.tokenHash = lw_token_hash32(node.token);
    
    // Check for token collision (astronomically unlikely, but log loudly if it happens)
    for (auto& p : nodes_) {
        if (p.first == nodeId) continue;
        const node_entry_t& other = p.second;
        if (other.state != NODE_STATE_LOST && other.tokenHash != 0 && other.tokenHash == node.tokenHash) {
            P3_FAIL("HRG_TOKEN_COLLISION",
                    "nodeId=%u mac=%s tokenHash=0x%08X COLLIDES with nodeId=%u mac=%s",
                    nodeId, node.mac, node.tokenHash, p.first, other.mac);
        }
    }
    
    // Populate WELCOME message
    welcome->nodeId = nodeId;
    strncpy(welcome->token, node.token, sizeof(welcome->token) - 1);
    welcome->udpPort = LW_UDP_PORT;
    welcome->hubEpoch_us = lw_monotonic_us();
    
    // Transition to AUTHED
    node.state = NODE_STATE_AUTHED;
    node.lastSeen_ms = millis();
    
    P3_PASS("HRG_WELCOME",
            "nodeId=%u mac=%s ip=%s tokenHash=0x%08X state=PENDING->AUTHED",
            nodeId, node.mac, node.ip, node.tokenHash);
    
    if (eventCallback_) {
        char msg[100];
        snprintf(msg, sizeof(msg), "AUTHED token=0x%08X", node.tokenHash);
        eventCallback_(nodeId, EVENT_NODE_AUTHED, msg);
    }
    
    return true;
}

void HubRegistry::updateKeepalive(uint8_t nodeId, const lw_msg_keepalive_t* ka) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        P3_FAIL("HRG_KA_UNKNOWN", "nodeId=%u (keepalive for unknown node)", nodeId);
        return;
    }
    
    node_entry_t& node = it->second;
    node.lastSeen_ms = millis();
    node.rssi = ka->rssi;
    node.loss_pct = ka->loss_pct;
    node.drift_us = ka->drift_us;
    node.keepalives_received++;
    
    // Check degradation criteria
    if (node.state == NODE_STATE_READY) {
        if (ka->loss_pct > 200 || abs(ka->drift_us) > LW_DRIFT_DEGRADED_US) {
            markDegraded(nodeId);
        }
    }
    
    // Periodic keepalive health log (throttled to every 15 seconds per node).
    // Must be heap-free: keepalives arrive in async_tcp context.
    static uint32_t lastKaLogMs[LW_MAX_NODES + 1] = {0};
    uint32_t* last = (nodeId <= LW_MAX_NODES) ? &lastKaLogMs[nodeId] : nullptr;
    if (last && p3_every_ms(last, 15000)) {
        P3_PASS("HRG_KA",
                "nodeId=%u mac=%s rssi=%d loss=%u.%02u%% drift=%d state=%s kaCount=%lu",
                nodeId, node.mac, node.rssi,
                node.loss_pct / 100, node.loss_pct % 100, node.drift_us,
                node_state_str(node.state),
                (unsigned long)node.keepalives_received);
    }
}

void HubRegistry::markReady(uint8_t nodeId) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) return;
    
    node_entry_t& node = it->second;
    if (node.state == NODE_STATE_AUTHED || node.state == NODE_STATE_DEGRADED) {
        node.state = NODE_STATE_READY;
        LW_LOGI("Node %d READY (loss=%d.%02d%%, drift=%d us)",
                nodeId, node.loss_pct / 100, node.loss_pct % 100, node.drift_us);
        
        if (eventCallback_) {
            char msg[100];
            snprintf(msg, sizeof(msg), "READY loss=%u.%02u%% drift=%dus", 
                     node.loss_pct / 100, node.loss_pct % 100, node.drift_us);
            eventCallback_(nodeId, EVENT_NODE_READY, msg);
        }
    }
}

void HubRegistry::markDegraded(uint8_t nodeId) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) return;
    
    node_entry_t& node = it->second;
    if (node.state == NODE_STATE_READY) {
        node.state = NODE_STATE_DEGRADED;
        LW_LOGW("Node %d DEGRADED (loss=%d.%02d%%, drift=%d us)",
                nodeId, node.loss_pct / 100, node.loss_pct % 100, node.drift_us);
        
        if (eventCallback_) {
            char msg[100];
            snprintf(msg, sizeof(msg), "DEGRADED loss=%u.%02u%% drift=%dus", 
                     node.loss_pct / 100, node.loss_pct % 100, node.drift_us);
            eventCallback_(nodeId, EVENT_NODE_DEGRADED, msg);
        }
    }
}

void HubRegistry::markLost(uint8_t nodeId) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) return;
    
    node_entry_t& node = it->second;
    const uint64_t lastSeenAgo_ms = millis() - node.lastSeen_ms;
    node.state = NODE_STATE_LOST;
    LW_LOGW("Node %d LOST (lastSeenAgo=%llu ms, udpSent=%lu, kaCount=%lu)",
            nodeId,
            (unsigned long long)lastSeenAgo_ms,
            (unsigned long)node.udp_sent,
            (unsigned long)node.keepalives_received);
    
    if (eventCallback_) {
        char msg[100];
        snprintf(msg, sizeof(msg), "LOST lastSeenAgo=%llu ms", (unsigned long long)lastSeenAgo_ms);
        eventCallback_(nodeId, EVENT_NODE_LOST, msg);
    }
}

void HubRegistry::setOtaState(uint8_t nodeId, const char* state, uint8_t pct, const char* version, const char* error) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) return;
    
    node_entry_t& node = it->second;
    strlcpy(node.ota_state, state, sizeof(node.ota_state));
    node.ota_pct = pct;
    if (version) strlcpy(node.ota_version, version, sizeof(node.ota_version));
    if (error) strlcpy(node.ota_error, error, sizeof(node.ota_error));
    
    LW_LOGI("Node %d OTA: state=%s pct=%u version=%s", nodeId, state, pct, version ? version : "");
}

node_entry_t* HubRegistry::getNode(uint8_t nodeId) {
    auto it = nodes_.find(nodeId);
    return (it != nodes_.end()) ? &it->second : nullptr;
}

bool HubRegistry::isReady(uint8_t nodeId) {
    auto it = nodes_.find(nodeId);
    return (it != nodes_.end() && it->second.state == NODE_STATE_READY);
}

uint8_t HubRegistry::getReadyCount() {
    uint8_t count = 0;
    for (const auto& pair : nodes_) {
        if (pair.second.state == NODE_STATE_READY) count++;
    }
    return count;
}

uint8_t HubRegistry::getTotalCount() {
    return nodes_.size();
}

void HubRegistry::tick(uint64_t now_ms) {
    // Check for keepalive timeouts
    for (auto& pair : nodes_) {
        node_entry_t& node = pair.second;
        if (node.state == NODE_STATE_LOST) continue;
        
        uint64_t since_last = now_ms - node.lastSeen_ms;
        if (since_last > LW_KEEPALIVE_TIMEOUT_MS) {
            P3_FAIL("HRG_KA_TIMEOUT",
                    "nodeId=%u mac=%s state=%s lastSeenAgo=%llu ms (timeout=%u ms)",
                    pair.first, node.mac, node_state_str(node.state),
                    (unsigned long long)since_last, (unsigned)LW_KEEPALIVE_TIMEOUT_MS);
            markLost(pair.first);
        }
    }
    
    // Cleanup LOST nodes
    cleanupLostNodes(now_ms);
    
    // Periodic invariant checks (every 10 seconds). Pass logging is throttled to avoid
    // flooding the serial monitor; failures always log immediately.
    static uint32_t lastInv = 0;
    if (p3_every_ms(&lastInv, 10000)) {
        bool ok = true;

        // Invariant A: tokenHash must be 0 in PENDING; must be nonzero in AUTHED/READY/DEGRADED
        for (auto& p : nodes_) {
            const uint8_t id = p.first;
            const node_entry_t& n = p.second;

            if (n.state == NODE_STATE_PENDING) {
                if (n.tokenHash != 0) {
                    ok = false;
                    P3_FAIL("HRG_INV_TOKEN_PENDING",
                            "nodeId=%u mac=%s state=PENDING but tokenHash=0x%08X (expected 0)",
                            id, n.mac, n.tokenHash);
                }
            } else if (n.state == NODE_STATE_AUTHED || n.state == NODE_STATE_READY || n.state == NODE_STATE_DEGRADED) {
                if (n.tokenHash == 0 || n.token[0] == '\0') {
                    ok = false;
                    P3_FAIL("HRG_INV_TOKEN_AUTHED",
                            "nodeId=%u mac=%s state=%s but token/tokenHash not set (tokenHash=0x%08X)",
                            id, n.mac, node_state_str(n.state), n.tokenHash);
                }
            }
        }

        // Invariant B: tokenHash uniqueness among non-LOST nodes
        for (auto it1 = nodes_.begin(); it1 != nodes_.end(); ++it1) {
            const node_entry_t& a = it1->second;
            if (a.state == NODE_STATE_LOST || a.tokenHash == 0) continue;

            for (auto it2 = std::next(it1); it2 != nodes_.end(); ++it2) {
                const node_entry_t& b = it2->second;
                if (b.state == NODE_STATE_LOST || b.tokenHash == 0) continue;

                if (a.tokenHash == b.tokenHash) {
                    ok = false;
                    P3_FAIL("HRG_INV_TOKEN_UNIQ",
                            "tokenHash collision: nodeId=%u mac=%s and nodeId=%u mac=%s tokenHash=0x%08X",
                            it1->first, a.mac, it2->first, b.mac, a.tokenHash);
                }
            }
        }

        if (ok) {
            static uint32_t lastOkLog = 0;
            static uint8_t lastNodes = 0;
            static uint8_t lastReady = 0;
            static uint8_t lastStations = 0xFF;

            const uint8_t nodes = (uint8_t)getTotalCount();
            const uint8_t ready = (uint8_t)getReadyCount();
            const uint8_t stations = getSoftApStationCount();
            const bool changed = (nodes != lastNodes) || (ready != lastReady) || (stations != lastStations);

            if (changed || p3_every_ms(&lastOkLog, 60000)) {
                P3_PASS("HRG_INV_OK", "nodes=%u ready=%u", (unsigned)nodes, (unsigned)ready);
                P3_PASS("HRG_STA", "softapStations=%u", (unsigned)stations);
                lastNodes = nodes;
                lastReady = ready;
                lastStations = stations;
            }
        }
    }
}

void HubRegistry::forEachReady(void (*callback)(node_entry_t* node, void* ctx), void* ctx) {
    for (auto& pair : nodes_) {
        if (pair.second.state == NODE_STATE_READY) {
            callback(&pair.second, ctx);
        }
    }
}

void HubRegistry::forEachAuthed(void (*callback)(node_entry_t* node, void* ctx), void* ctx) {
    for (auto& pair : nodes_) {
        // Include AUTHED, READY, and DEGRADED (all authenticated states)
        if (pair.second.state >= NODE_STATE_AUTHED && pair.second.state <= NODE_STATE_DEGRADED) {
            callback(&pair.second, ctx);
        }
    }
}

void HubRegistry::forEachAll(void (*callback)(node_entry_t* node, void* ctx), void* ctx) {
    for (auto& pair : nodes_) {
        callback(&pair.second, ctx);
    }
}

void HubRegistry::generateToken(char* token_out, size_t len) {
    snprintf(token_out, len, "tok_%lu_%lu", millis(), (unsigned long)nextToken_++);
}

void HubRegistry::cleanupLostNodes(uint64_t now_ms) {
    auto it = nodes_.begin();
    while (it != nodes_.end()) {
        if (it->second.state == NODE_STATE_LOST) {
            uint64_t since_lost = now_ms - it->second.lastSeen_ms;
            if (since_lost > LW_CLEANUP_TIMEOUT_MS) {
                P3_PASS("HRG_CLEANUP", "nodeId=%u mac=%s (LOST->ERASED)", it->first, it->second.mac);
                it = nodes_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

const char* node_state_str(node_state_t state) {
    switch (state) {
        case NODE_STATE_PENDING: return "PENDING";
        case NODE_STATE_AUTHED: return "AUTHED";
        case NODE_STATE_READY: return "READY";
        case NODE_STATE_DEGRADED: return "DEGRADED";
        case NODE_STATE_LOST: return "LOST";
        default: return "UNKNOWN";
    }
}
