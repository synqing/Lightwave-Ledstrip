/**
 * Hub HTTP + WebSocket Server Implementation
 * Uses ESPAsyncWebServer (Arduino-compatible)
 */

#include "hub_http_ws.h"
#include "../../common/util/logging.h"
#include "../../common/clock/monotonic.h"
#include "../../common/proto/proto_constants.h"
#include "../../common/proto/ws_messages.h"
#include <ArduinoJson.h>

#define LW_LOG_TAG "HubHttpWs"

static bool sendWsText(AsyncWebSocket& ws, uint32_t clientId, const char* payload) {
    if (!payload) return false;
    AsyncWebSocketClient* client = ws.client(clientId);
    if (!client || client->status() != WS_CONNECTED) return false;

    // Hard guardrail: never allow the ESPAsyncWebServer queue to fill and close the connection.
    // When a client is slow, we prefer dropping intermediate state updates (HubState is authoritative and
    // the next batch will contain the latest values) over disconnect/reconnect churn.
    if (!client->canSend() || client->queueIsFull()) {
        static uint32_t s_dropCount = 0;
        static uint32_t s_lastLogMs = 0;
        s_dropCount++;

        const uint32_t nowMs = millis();
        if (nowMs - s_lastLogMs >= 1000) {
            LW_LOGW("WS backpressure: drop=%lu client=%u queueLen=%u",
                    (unsigned long)s_dropCount,
                    (unsigned)clientId,
                    (unsigned)client->queueLen());
            s_lastLogMs = nowMs;
        }
        return false;
    }

    return client->text(payload);
}

HubHttpWs::HubHttpWs(HubRegistry* registry, hub_clock_t* clock)
    : registry_(registry), clock_(clock), server_(LW_CTRL_HTTP_PORT), ws_(LW_WS_PATH),
      otaRepo_(nullptr), otaDispatch_(nullptr), state_(nullptr),
      pendingHead_(0), pendingTail_(0), pendingCount_(0) {
}

bool HubHttpWs::init(uint16_t port) {
    // Register /health endpoint
    server_.on("/health", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["proto"] = LW_PROTO_VER;
        doc["uptime_s"] = hub_clock_uptime_s(clock_);
        doc["nodes_total"] = registry_->getTotalCount();
        doc["nodes_ready"] = registry_->getReadyCount();
        doc["tick_hz"] = LW_UDP_TICK_HZ;
        
        String response;
        serializeJson(doc, response);
        
        request->send(200, "application/json", response);
    });
    
    // Register /metrics endpoint (with aggregates)
    server_.on("/metrics", HTTP_GET, [this](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        
        // Compute aggregates
        struct {
            int count = 0;
            int ready = 0;
            int pending = 0;
            int authed = 0;
            int degraded = 0;
            int lost = 0;
            int totalRssi = 0;
            int totalLoss = 0;
            int worstLoss = 0;
            int worstDrift = 0;
        } stats;
        
        registry_->forEachAll([](node_entry_t* node, void* ctx) {
            auto* s = (decltype(stats)*)ctx;
            s->count++;
            switch (node->state) {
                case NODE_STATE_PENDING: s->pending++; break;
                case NODE_STATE_AUTHED: s->authed++; break;
                case NODE_STATE_READY: s->ready++; break;
                case NODE_STATE_DEGRADED: s->degraded++; break;
                case NODE_STATE_LOST: s->lost++; break;
            }
            if (node->rssi != 0) s->totalRssi += node->rssi;
            s->totalLoss += node->loss_pct;
            if (node->loss_pct > s->worstLoss) s->worstLoss = node->loss_pct;
            if (abs(node->drift_us) > abs(s->worstDrift)) s->worstDrift = node->drift_us;
        }, &stats);
        
        response->printf("{\"uptime_s\":%lu,", hub_clock_uptime_s(clock_));
        response->printf("\"tick_count\":%lu,", (unsigned long)clock_->tick_count);
        response->printf("\"tick_overruns\":%lu,", (unsigned long)clock_->tick_overruns);
        response->printf("\"nodes\":{\"total\":%d,\"ready\":%d,\"pending\":%d,\"authed\":%d,\"degraded\":%d,\"lost\":%d},",
                        stats.count, stats.ready, stats.pending, stats.authed, stats.degraded, stats.lost);
        response->printf("\"avg_rssi\":%d,", stats.count > 0 ? stats.totalRssi / stats.count : 0);
        response->printf("\"avg_loss_pct\":%d,", stats.count > 0 ? stats.totalLoss / stats.count : 0);
        response->printf("\"worst_loss_pct\":%d,", stats.worstLoss);
        response->printf("\"worst_drift_us\":%d}", stats.worstDrift);
        
        request->send(response);
    });
    
    // Register /nodes endpoint (detailed per-node snapshot)
    server_.on("/nodes", HTTP_GET, [this](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        response->print("[");
        
        struct NodeIterCtx {
            AsyncResponseStream* resp;
            bool* first;
            uint64_t now_ms;
        };
        
        bool first = true;
        NodeIterCtx ctx = { response, &first, (uint64_t)millis() };
        
        registry_->forEachAll([](node_entry_t* node, void* ctxPtr) {
            NodeIterCtx* args = (NodeIterCtx*)ctxPtr;
            
            if (!*args->first) args->resp->print(",");
            *args->first = false;
            
            uint64_t age_ms = args->now_ms - node->lastSeen_ms;
            
            args->resp->printf("{\"id\":%u,", node->nodeId);
            args->resp->printf("\"mac\":\"%s\",", node->mac);
            args->resp->printf("\"ip\":\"%s\",", node->ip);
            args->resp->printf("\"fw\":\"%s\",", node->fw);
            args->resp->printf("\"state\":\"%s\",", node_state_str(node->state));
            args->resp->printf("\"tokenHash\":%lu,", (unsigned long)node->tokenHash);
            args->resp->printf("\"age_ms\":%llu,", (unsigned long long)age_ms);
            args->resp->printf("\"rssi\":%d,", node->rssi);
            args->resp->printf("\"loss_pct\":%u,", node->loss_pct);
            args->resp->printf("\"drift_us\":%d,", node->drift_us);
            args->resp->printf("\"udp_sent\":%lu,", (unsigned long)node->udp_sent);
            args->resp->printf("\"keepalives\":%lu}", (unsigned long)node->keepalives_received);
        }, &ctx);
        
        response->print("]");
        request->send(response);
    });
    
    // OTA Endpoints (require otaRepo_ and otaDispatch_ to be set via setOta())
    // GET /ota/debug - Filesystem diagnostic
    server_.on("/ota/debug", HTTP_GET, [this](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        response->print("{");
        response->printf("\"littlefs_mounted\":%s,", LittleFS.begin() ? "true" : "false");
        response->printf("\"manifest_exists\":%s,", LittleFS.exists("/ota/manifest.json") ? "true" : "false");
        
        if (LittleFS.exists("/ota/manifest.json")) {
            File f = LittleFS.open("/ota/manifest.json", "r");
            response->printf("\"manifest_size\":%d,", f ? (int)f.size() : 0);
            if (f) f.close();
        } else {
            response->print("\"manifest_size\":0,");
        }
        
        response->printf("\"ota_repo_init\":%s", otaRepo_ ? "true" : "false");
        response->print("}");
        request->send(response);
    });
    
    // GET /ota/manifest.json
    server_.on("/ota/manifest.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!otaRepo_) {
            request->send(503, "text/plain", "OTA not initialized");
            return;
        }
        
        // Check if file exists before serving
        if (!LittleFS.exists("/ota/manifest.json")) {
            request->send(404, "text/plain", "Manifest not found in filesystem");
            return;
        }
        
        // Serve manifest file from LittleFS
        request->send(LittleFS, "/ota/manifest.json", "application/json");
    });
    
    // POST /ota/rollout?track=stable&node=1&node=2...
    server_.on("/ota/rollout", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!otaDispatch_) {
            request->send(503, "text/plain", "OTA dispatcher not initialized");
            return;
        }
        
        if (!request->hasParam("track")) {
            request->send(400, "text/plain", "Missing 'track' parameter");
            return;
        }
        
        String track = request->getParam("track")->value();
        
        // Parse node IDs from query params
        std::vector<uint8_t> nodeIds;
        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* p = request->getParam(i);
            if (strcmp(p->name().c_str(), "node") == 0) {
                nodeIds.push_back((uint8_t)p->value().toInt());
            }
        }
        
        if (nodeIds.empty()) {
            request->send(400, "text/plain", "No nodes specified");
            return;
        }
        
        if (otaDispatch_->startRollout(track.c_str(), nodeIds)) {
            request->send(200, "text/plain", "Rollout started");
        } else {
            request->send(500, "text/plain", "Failed to start rollout");
        }
    });
    
    // POST /ota/abort
    server_.on("/ota/abort", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!otaDispatch_) {
            request->send(503, "text/plain", "OTA dispatcher not initialized");
            return;
        }
        otaDispatch_->abort();
        request->send(200, "text/plain", "Rollout aborted");
    });
    
    // GET /ota/state
    server_.on("/ota/state", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!otaDispatch_) {
            request->send(503, "text/plain", "OTA dispatcher not initialized");
            return;
        }
        
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        response->printf("{\"state\":\"%s\",",
                        otaDispatch_->getState() == OTA_DISPATCH_IDLE ? "idle" :
                        otaDispatch_->getState() == OTA_DISPATCH_IN_PROGRESS ? "in_progress" :
                        otaDispatch_->getState() == OTA_DISPATCH_COMPLETE ? "complete" : "aborted");
        response->printf("\"currentNode\":%u,", otaDispatch_->getCurrentNode());
        response->printf("\"completed\":%u,", otaDispatch_->getCompletedCount());
        response->printf("\"total\":%u}", otaDispatch_->getTotalCount());
        request->send(response);
    });
    
    // Serve OTA binaries (via onNotFound fallback)
    server_.onNotFound([this](AsyncWebServerRequest *request) {
        String path = request->url();
        
        // Check if this is an OTA binary request
        if (path.startsWith("/ota/") && path.endsWith(".bin") && otaRepo_) {
            if (otaRepo_->validateBinaryPath(path.c_str())) {
                String fsPath = otaRepo_->urlToFsPath(path.c_str());
                request->send(LittleFS, fsPath, "application/octet-stream");
                return;
            }
        }
        
        request->send(404, "text/plain", "Not Found");
    });
    
    // Register WebSocket handler
    ws_.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWsEvent(server, client, type, arg, data, len);
    });
    
    server_.addHandler(&ws_);
    
    server_.begin();
    
    LW_LOGI("HTTP + WS server started on port %d", port);
    return true;
}

void HubHttpWs::loop() {
    // Clean up disconnected clients
    ws_.cleanupClients();
    processPendingJoins(2);
}

void HubHttpWs::setOta(HubOtaRepo* repo, HubOtaDispatch* dispatch) {
    otaRepo_ = repo;
    otaDispatch_ = dispatch;
    LW_LOGI("OTA repository and dispatcher linked to HTTP/WS server");
}

void HubHttpWs::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                          AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            LW_LOGI("WS client %u connected from %s", client->id(), client->remoteIP().toString().c_str());
            // Critical: avoid disconnect storms under load (encoder spam, weak link, etc.).
            // Dropping intermediate messages is strictly better than forcing rejoin+snapshot loops.
            client->setCloseClientOnQueueFull(false);
            break;
            
        case WS_EVT_DISCONNECT:
            LW_LOGI("WS client %u disconnected", client->id());
            
            // Remove from client tracking
            if (clients_.count(client->id())) {
                uint8_t nodeId = clients_[client->id()];
                clients_.erase(client->id());
                
                // Mark node as LOST in registry
                LW_LOGW("Node %d (client %u) disconnected, marking as LOST", nodeId, client->id());
                registry_->markLost(nodeId);
            }
            break;
            
        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            
            // Only handle complete text frames
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                // Parse JSON once (deserialize directly from data buffer)
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, data, len);
                
                if (error) {
                    LW_LOGE("JSON parse error: %s", error.c_str());
                    return;
                }
                
                const char* msgType = doc["t"];
                if (!msgType) {
                    LW_LOGE("Missing message type 't'");
                    return;
                }
                
                LW_LOGD("WS message from client %u: type=%s, %u bytes",
                        client->id(), msgType, (unsigned)len);
                
                // Dispatch to handlers (passing parsed doc, not raw buffer)
                if (strcmp(msgType, "hello") == 0) {
                    handleHello(client, doc);
                } else if (strcmp(msgType, "ka") == 0) {
                    handleKeepalive(client, doc);
                } else if (strcmp(msgType, "ts_ping") == 0) {
                    handleTsPing(client, doc);
                } else if (strcmp(msgType, "ota_status") == 0) {
                    handleOtaStatus(client, doc);
                } else {
                    LW_LOGW("Unknown message type: %s", msgType);
                }
            }
            break;
        }
        
        case WS_EVT_PONG:
            break;
            
        case WS_EVT_ERROR:
            LW_LOGE("WS error from client %u", client->id());
            break;
    }
}

bool HubHttpWs::enqueuePendingJoin(uint32_t clientId, uint8_t nodeId, uint64_t applyAt_us) {
    if (pendingCount_ >= kPendingJoinMax) {
        return false;
    }

    PendingJoin& slot = pendingJoins_[pendingTail_];
    slot.clientId = clientId;
    slot.nodeId = nodeId;
    slot.applyAt_us = applyAt_us;
    pendingTail_ = (pendingTail_ + 1) % kPendingJoinMax;
    pendingCount_++;
    return true;
}

void HubHttpWs::processPendingJoins(uint8_t maxPerLoop) {
    uint8_t processed = 0;
    while (pendingCount_ > 0 && processed < maxPerLoop) {
        PendingJoin& slot = pendingJoins_[pendingHead_];
        sendWelcome(slot.clientId, slot.nodeId);
        sendStateSnapshot(slot.clientId, slot.nodeId, slot.applyAt_us);
        pendingHead_ = (pendingHead_ + 1) % kPendingJoinMax;
        pendingCount_--;
        processed++;
    }
}

void HubHttpWs::handleHello(AsyncWebSocketClient *client, JsonDocument& doc) {
    lw_msg_hello_t hello = {};
    strlcpy(hello.mac, doc["mac"] | "", sizeof(hello.mac));
    strlcpy(hello.fw, doc["fw"] | "", sizeof(hello.fw));
    hello.caps.udp = doc["caps"]["udp"] | false;
    hello.caps.ota = doc["caps"]["ota"] | false;
    hello.caps.clock = doc["caps"]["clock"] | false;
    hello.topo.leds = doc["topo"]["leds"] | 0;
    hello.topo.channels = doc["topo"]["channels"] | 0;
    
    // Register node (use client IP)
    String ip = client->remoteIP().toString();
    uint8_t nodeId = registry_->registerNode(&hello, ip.c_str());
    
    if (nodeId == 0) {
        LW_LOGE("Failed to register node %s", hello.mac);
        return;
    }
    
    // Track client ID -> nodeId
    clients_[client->id()] = nodeId;

    // Initialise per-node HubState defaults on join/rejoin
    if (state_) {
        state_->onNodeRegistered(nodeId);
    }

    LW_LOGI("Node %d (%s) registered from %s, client ID %u", nodeId, hello.mac, ip.c_str(), client->id());
    
    // Defer WELCOME + snapshot to the main loop so async_tcp never blocks.
    const uint64_t applyAt_us = hub_clock_now_us(clock_) + LW_APPLY_AHEAD_US;
    if (!enqueuePendingJoin(client->id(), nodeId, applyAt_us)) {
        LW_LOGW("Pending join queue full, sending WELCOME inline for node %d", nodeId);
        sendWelcome(client->id(), nodeId);
        sendStateSnapshot(client->id(), nodeId, applyAt_us);
    }
}

void HubHttpWs::handleKeepalive(AsyncWebSocketClient *client, JsonDocument& doc) {
    lw_msg_keepalive_t ka = {};
    ka.nodeId = doc["nodeId"] | 0;
    strlcpy(ka.token, doc["token"] | "", sizeof(ka.token));
    ka.rssi = doc["rssi"] | 0;
    ka.loss_pct = doc["loss_pct"] | 0;
    ka.drift_us = doc["drift_us"] | 0;
    ka.uptime_s = doc["uptime_s"] | 0;
    
    // Verify client ID matches stored nodeId
    if (clients_.count(client->id()) && clients_[client->id()] != ka.nodeId) {
        LW_LOGW("Keepalive from client %u with mismatched nodeId %d (expected %d)",
                client->id(), ka.nodeId, clients_[client->id()]);
        return;
    }
    
    registry_->updateKeepalive(ka.nodeId, &ka);
    
    // Check if node should transition to READY
    node_entry_t* node = registry_->getNode(ka.nodeId);
    if (node && node->state == NODE_STATE_AUTHED) {
        registry_->markReady(ka.nodeId);
        LW_LOGI("Node %d transitioned to READY", ka.nodeId);
    }
}

void HubHttpWs::handleTsPing(AsyncWebSocketClient *client, JsonDocument& doc) {
    uint8_t nodeId = doc["nodeId"] | 0;
    uint32_t seq = doc["seq"] | 0;
    uint64_t t1_us = doc["t1_us"] | 0ULL;
    
    // Verify client ID matches stored nodeId
    if (clients_.count(client->id()) && clients_[client->id()] != nodeId) {
        LW_LOGW("TsPing from client %u with mismatched nodeId %d (expected %d)",
                client->id(), nodeId, clients_[client->id()]);
        return;
    }
    
    uint64_t t2_us = lw_monotonic_us();  // Hub receive timestamp
    sendTsPong(client->id(), nodeId, seq, t1_us, t2_us);
}

void HubHttpWs::sendWelcome(uint32_t clientId, uint8_t nodeId) {
    lw_msg_welcome_t welcome;
    if (!registry_->sendWelcome(nodeId, &welcome)) {
        LW_LOGE("Failed to prepare WELCOME for node %d", nodeId);
        return;
    }
    
    StaticJsonDocument<256> doc;
    doc["t"] = "welcome";
    doc["proto"] = LW_PROTO_VER;
    doc["nodeId"] = welcome.nodeId;
    doc["token"] = welcome.token;
    doc["udpPort"] = welcome.udpPort;
    doc["hubEpoch_us"] = welcome.hubEpoch_us;

    char buf[256];
    size_t outLen = serializeJson(doc, buf, sizeof(buf) - 1);
    if (outLen == 0 || outLen >= sizeof(buf)) {
        LW_LOGE("Failed to serialise WELCOME (nodeId=%u)", (unsigned)nodeId);
        return;
    }
    buf[outLen] = '\0';

    AsyncWebSocketClient *client = ws_.client(clientId);
    if (client && client->status() == WS_CONNECTED) {
        client->text(buf);
        LW_LOGI("Sent WELCOME to node %d (client %u)", nodeId, clientId);
    } else {
        LW_LOGE("Failed to send WELCOME: client %u not connected", clientId);
    }
}

void HubHttpWs::sendStateSnapshot(uint32_t clientId, uint8_t nodeId, uint64_t applyAt_us) {
    if (!state_) return;

    const HubState::FullSnapshot snap = state_->createFullSnapshot(nodeId);
    const bool zonesEnabled = state_->areZonesEnabled();

    StaticJsonDocument<1024> doc;
    doc["type"] = "state.snapshot";
    doc["nodeId"] = nodeId;
    doc["applyAt_us"] = (unsigned long long)applyAt_us;
    doc["zonesEnabled"] = zonesEnabled;

    JsonObject global = doc.createNestedObject("global");
    global["effectId"] = snap.global.effectId;
    global["brightness"] = snap.global.brightness;
    global["speed"] = snap.global.speed;
    global["paletteId"] = snap.global.paletteId;
    global["hue"] = snap.global.hue;
    global["intensity"] = snap.global.intensity;
    global["saturation"] = snap.global.saturation;
    global["complexity"] = snap.global.complexity;
    global["variation"] = snap.global.variation;

    if (zonesEnabled) {
        JsonArray zones = doc.createNestedArray("zones");
        for (uint8_t z = 0; z < HubState::MAX_ZONES; ++z) {
            JsonObject zone = zones.createNestedObject();
            zone["zoneId"] = z;
            zone["effectId"] = snap.zones[z].effectId;
            zone["brightness"] = snap.zones[z].brightness;
            zone["speed"] = snap.zones[z].speed;
            zone["paletteId"] = snap.zones[z].paletteId;
            zone["blendMode"] = snap.zones[z].blendMode;
        }
    }

    char buf[1024];
    size_t len = serializeJson(doc, buf, sizeof(buf) - 1);
    if (len == 0 || len >= sizeof(buf)) {
        LW_LOGE("State snapshot too large, dropping (nodeId=%u)", (unsigned)nodeId);
        return;
    }
    buf[len] = '\0';

    if (sendWsText(ws_, clientId, buf)) {
        LW_LOGI("Sent state snapshot to node %u (client %u)", (unsigned)nodeId, (unsigned)clientId);
    } else {
        LW_LOGE("Failed to send state snapshot: client %u not connected", (unsigned)clientId);
    }
}

void HubHttpWs::broadcastGlobalDelta(uint16_t dirtyMask, const HubState::GlobalParams& global, uint64_t applyAt_us) {
    if (dirtyMask == 0) return;

    // Effect changes are sent via effects.setCurrent.
    if (dirtyMask & HubState::GF_EFFECT) {
        StaticJsonDocument<192> doc;
        doc["type"] = "effects.setCurrent";
        doc["effectId"] = global.effectId;
        doc["applyAt_us"] = (unsigned long long)applyAt_us;

        char buf[192];
        size_t len = serializeJson(doc, buf, sizeof(buf) - 1);
        if (len > 0 && len < sizeof(buf)) {
            buf[len] = '\0';
            struct Ctx { HubHttpWs* self; const char* msg; };
            Ctx ctx{this, buf};
            registry_->forEachReady([](node_entry_t* node, void* ctxPtr) {
                auto* c = (Ctx*)ctxPtr;
                // Find clientId for this nodeId
                uint32_t clientId = 0;
                for (const auto& p : c->self->clients_) {
                    if (p.second == node->nodeId) { clientId = p.first; break; }
                }
                if (clientId) sendWsText(c->self->ws_, clientId, c->msg);
            }, &ctx);
        }
    }

    // Remaining parameter fields are sent via parameters.set (additive fields only).
    const uint16_t paramMask = dirtyMask & ~(uint16_t)HubState::GF_EFFECT;
    if (paramMask == 0) return;

    StaticJsonDocument<256> doc;
    doc["type"] = "parameters.set";
    doc["applyAt_us"] = (unsigned long long)applyAt_us;
    if (dirtyMask & HubState::GF_BRIGHTNESS) doc["brightness"] = global.brightness;
    if (dirtyMask & HubState::GF_SPEED) doc["speed"] = global.speed;
    if (dirtyMask & HubState::GF_PALETTE) doc["paletteId"] = global.paletteId;
    if (dirtyMask & HubState::GF_HUE) doc["hue"] = global.hue;
    if (dirtyMask & HubState::GF_INTENSITY) doc["intensity"] = global.intensity;
    if (dirtyMask & HubState::GF_SATURATION) doc["saturation"] = global.saturation;
    if (dirtyMask & HubState::GF_COMPLEXITY) doc["complexity"] = global.complexity;
    if (dirtyMask & HubState::GF_VARIATION) doc["variation"] = global.variation;

    char buf[256];
    size_t len = serializeJson(doc, buf, sizeof(buf) - 1);
    if (len == 0 || len >= sizeof(buf)) {
        LW_LOGE("parameters.set too large, dropping");
        return;
    }
    buf[len] = '\0';

    struct Ctx { HubHttpWs* self; const char* msg; };
    Ctx ctx{this, buf};
    registry_->forEachReady([](node_entry_t* node, void* ctxPtr) {
        auto* c = (Ctx*)ctxPtr;
        uint32_t clientId = 0;
        for (const auto& p : c->self->clients_) {
            if (p.second == node->nodeId) { clientId = p.first; break; }
        }
        if (clientId) sendWsText(c->self->ws_, clientId, c->msg);
    }, &ctx);
}

void HubHttpWs::sendZoneDelta(uint8_t nodeId, uint8_t zoneId, uint8_t dirtyMask, const HubState::ZoneSettings& zone, uint64_t applyAt_us) {
    if (nodeId == 0 || dirtyMask == 0) return;

    uint32_t clientId = 0;
    for (const auto& p : clients_) {
        if (p.second == nodeId) { clientId = p.first; break; }
    }
    if (clientId == 0) return;

    // Use a single zones.update message for coalesced changes.
    StaticJsonDocument<256> doc;
    doc["type"] = "zones.update";
    doc["zoneId"] = zoneId;
    doc["applyAt_us"] = (unsigned long long)applyAt_us;
    if (dirtyMask & HubState::ZF_EFFECT) doc["effectId"] = zone.effectId;
    if (dirtyMask & HubState::ZF_BRIGHTNESS) doc["brightness"] = zone.brightness;
    if (dirtyMask & HubState::ZF_SPEED) doc["speed"] = zone.speed;
    if (dirtyMask & HubState::ZF_PALETTE) doc["paletteId"] = zone.paletteId;
    if (dirtyMask & HubState::ZF_BLEND) doc["blendMode"] = zone.blendMode;

    char buf[256];
    size_t len = serializeJson(doc, buf, sizeof(buf) - 1);
    if (len == 0 || len >= sizeof(buf)) return;
    buf[len] = '\0';

    sendWsText(ws_, clientId, buf);
}

void HubHttpWs::sendTsPong(uint32_t clientId, uint8_t nodeId, uint32_t seq, uint64_t t1_us, uint64_t t2_us) {
    // Capture hub transmit timestamp as late as possible
    uint64_t t3_us = lw_monotonic_us();

    // Keep this handler heap-free (can run in async_tcp context).
    StaticJsonDocument<256> doc;
    doc["t"] = "ts_pong";
    doc["nodeId"] = nodeId;
    doc["seq"] = seq;
    doc["t1_us"] = (unsigned long long)t1_us;
    doc["t2_us"] = (unsigned long long)t2_us;
    doc["t3_us"] = (unsigned long long)t3_us;

    char buf[256];
    size_t len = serializeJson(doc, buf, sizeof(buf) - 1);
    if (len == 0 || len >= sizeof(buf)) {
        LW_LOGE("Failed to serialise ts_pong (nodeId=%u)", (unsigned)nodeId);
        return;
    }
    buf[len] = '\0';

    AsyncWebSocketClient *client = ws_.client(clientId);
    if (client && client->status() == WS_CONNECTED) {
        client->text(buf);
    } else {
        LW_LOGE("Failed to send ts_pong: client %u not connected", clientId);
    }
}

void HubHttpWs::sendOtaUpdate(uint8_t nodeId, const char* version, const char* url, const char* sha256) {
    // Find client ID for this node
    uint32_t clientId = 0;
    for (auto& pair : clients_) {
        if (pair.second == nodeId) {
            clientId = pair.first;
            break;
        }
    }
    
    if (clientId == 0) {
        LW_LOGE("Cannot send OTA update: node %u has no active WS client", nodeId);
        return;
    }
    
    // Heap-free: can run under async_tcp callbacks.
    StaticJsonDocument<384> doc;
    doc["t"] = "ota_update";
    doc["version"] = version;
    doc["url"] = url;
    doc["sha256"] = sha256;
    
    char buf[384];
    size_t len = serializeJson(doc, buf, sizeof(buf) - 1);
    if (len == 0 || len >= sizeof(buf)) {
        LW_LOGE("Failed to serialise ota_update (nodeId=%u)", (unsigned)nodeId);
        return;
    }
    buf[len] = '\0';
    
    AsyncWebSocketClient *client = ws_.client(clientId);
    if (client && client->status() == WS_CONNECTED) {
        client->text(buf);
        LW_LOGI("Sent OTA update to node %u (client %u): version=%s", nodeId, clientId, version);
    } else {
        LW_LOGE("Failed to send OTA update: client %u not connected", clientId);
    }
}

void HubHttpWs::handleOtaStatus(AsyncWebSocketClient *client, JsonDocument& doc) {
    uint8_t nodeId = doc["nodeId"] | 0;
    const char* state = doc["state"] | "unknown";
    uint8_t pct = doc["pct"] | 0;
    const char* error = doc["error"] | "";
    
    // Verify client ID matches stored nodeId
    if (clients_.count(client->id()) && clients_[client->id()] != nodeId) {
        LW_LOGW("OTA status from client %u with mismatched nodeId %d (expected %d)",
                client->id(), nodeId, clients_[client->id()]);
        return;
    }
    
    LW_LOGI("Node %u OTA status: state=%s pct=%u%s%s", 
            nodeId, state, pct, error[0] ? " error=" : "", error);
    
    // Forward to dispatcher
    if (otaDispatch_) {
        otaDispatch_->onNodeOtaStatus(nodeId, state, pct, error);
    }
}
