/**
 * Node WebSocket Control Plane Implementation (ESP-IDF Native)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_websocket_client.h>
#include "node_ws_ctrl.h"
#include "node_udp_rx.h"
#include "../ota/node_ota_client.h"
#include "../sync/node_timesync.h"
#include "../sync/node_scheduler.h"
#include "../../common/util/logging.h"
#include "../../common/proto/udp_packets.h"
#include "../../common/clock/monotonic.h"
#include "../../effects/zones/ZoneComposer.h"

extern lightwaveos::zones::ZoneComposer zoneComposer;

#define P3_PASS(code, fmt, ...) LW_LOGI("[P3-PASS][%s] " fmt, code, ##__VA_ARGS__)

NodeWsCtrl* NodeWsCtrl::instance_ = nullptr;

NodeWsCtrl::NodeWsCtrl()
    : ws_(nullptr), state_(WS_STATE_DISCONNECTED), udpRx_(nullptr), ota_(nullptr), timesync_(nullptr), scheduler_(nullptr),
      nodeId_(0), tokenHash_(0),
      lastKeepalive_ms_(0), lastTsPing_ms_(0), tsPingSeq_(0), wsCmdSeq_(0), started_(false),
      restartRequested_(false), lastRestartAttempt_ms_(0),
      onWelcome(nullptr), onTsPong(nullptr) {
    instance_ = this;
    token_[0] = '\0';
    hub_ip_[0] = '\0';
    lastEffectId_ = 0;
    lastPaletteId_ = 0;
}

NodeWsCtrl::~NodeWsCtrl() {
    if (ws_) {
        esp_websocket_client_stop(ws_);
        esp_websocket_client_destroy(ws_);
        ws_ = nullptr;
    }
}

bool NodeWsCtrl::init(const char* hub_ip) {
    // Save hub IP for deferred start
    strlcpy(hub_ip_, hub_ip, sizeof(hub_ip_));
    
    char uri[128];
    snprintf(uri, sizeof(uri), "ws://%s:%d/ws", hub_ip, LW_CTRL_HTTP_PORT);
    
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = uri;
    ws_cfg.disable_auto_reconnect = false;
    
    // Enable WebSocket ping/pong disconnect detection so a hub reboot is
    // detected quickly (TCP keepalive alone can take many seconds).
    ws_cfg.ping_interval_sec = 2;
    ws_cfg.disable_pingpong_discon = false;
    
    // TCP keepalive as transport-level safety net
    ws_cfg.keep_alive_enable = true;
    ws_cfg.keep_alive_idle = 5;
    ws_cfg.keep_alive_interval = 5;
    ws_cfg.keep_alive_count = 3;
    
    ws_ = esp_websocket_client_init(&ws_cfg);
    if (!ws_) {
        LW_LOGE("Failed to create WebSocket client");
        return false;
    }
    
    esp_websocket_register_events(ws_, WEBSOCKET_EVENT_ANY, wsEventHandler, (void*)this);
    // DON'T start yet - wait for WiFi to be ready
    
    LW_LOGI("WS client initialized: %s (will start when WiFi ready)", uri);
    return true;
}

bool NodeWsCtrl::recreateClient() {
    if (!ws_) return false;

    LW_LOGW("Recreating WS client (hub reboot recovery)...");
    esp_websocket_client_stop(ws_);
    esp_websocket_client_destroy(ws_);
    ws_ = nullptr;
    started_ = false;
    state_ = WS_STATE_DISCONNECTED;

    return init(hub_ip_);
}

void NodeWsCtrl::loop() {
    // Start WS client when WiFi is ready and we haven't started yet
    if (ws_ && !started_ && WiFi.status() == WL_CONNECTED) {
        esp_websocket_client_start(ws_);
        started_ = true;
        LW_LOGI("WS client starting (WiFi ready)...");
    }
    
    uint32_t now = millis();

    // If the hub rebooted, we can end up disconnected with no auto-reconnect attempts.
    // Force a restart of the WS client from the main loop (not the event callback).
    if (ws_ && started_ && restartRequested_ && WiFi.status() == WL_CONNECTED) {
        if (now - lastRestartAttempt_ms_ >= 1000) {
            lastRestartAttempt_ms_ = now;
            LW_LOGW("WS restart requested (reconnecting to hub)...");

            if (!recreateClient()) {
                LW_LOGW("WS recreate failed (will retry)");
                return;
            }

            esp_websocket_client_start(ws_);
            started_ = true;
            restartRequested_ = false;
        }
    }
    
    // Send HELLO when connected but not authenticated
    if (state_ == WS_STATE_CONNECTED) {
        sendHello();
        state_ = WS_STATE_HELLO_SENT;
    }
    
    // Send periodic KEEPALIVE
    if (state_ == WS_STATE_AUTHENTICATED) {
        if (now - lastKeepalive_ms_ >= LW_KEEPALIVE_PERIOD_MS) {
            sendKeepalive();
            lastKeepalive_ms_ = now;
        }
        
        // Every 5s, re-emit auth state so you can never "miss the boot"
        static uint32_t lastAuthPass = 0;
        if (now - lastAuthPass >= 5000) {
            P3_PASS("NWS_AUTH", "nodeId=%u tokenHash=0x%08X wsConnected=%d",
                    (unsigned)nodeId_, (unsigned)tokenHash_, (int)esp_websocket_client_is_connected(ws_));
            lastAuthPass = now;
        }
        
        // WS ts_ping removed - now using dedicated UDP time-sync
    } else {
        // Log state if not authenticated
        static uint32_t lastStateLog = 0;
        if (now - lastStateLog >= 2000) {
            LW_LOGD("WS state: %d (not authenticated)", state_);
            lastStateLog = now;
        }
    }
}

void NodeWsCtrl::wsEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    NodeWsCtrl* self = (NodeWsCtrl*)handler_args;
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    
    if (self->verbose_) {
        Serial.printf("[DEBUG-NODE] WS Event: id=%d\n", event_id);
    }
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            LW_LOGI("WS connected to hub");
            if (self->verbose_) Serial.printf("[DEBUG-NODE] WEBSOCKET_EVENT_CONNECTED\n");
            self->state_ = WS_STATE_CONNECTED;
            break;
            
        case WEBSOCKET_EVENT_DISCONNECTED:
            LW_LOGW("WS disconnected");
            if (self->verbose_) Serial.printf("[DEBUG-NODE] WEBSOCKET_EVENT_DISCONNECTED\n");
            {
                self->state_ = WS_STATE_DISCONNECTED;
                self->restartRequested_ = true;
            }
            
            // Disarm fanout immediately - forces rejection until new WELCOME
            if (self->udpRx_) {
                self->udpRx_->setTokenHash(0);
                P3_PASS("NWS_DISARM", "fanout disarmed on WS disconnect");
            }
            break;
            
        case WEBSOCKET_EVENT_DATA:
            if (self->verbose_) {
                Serial.printf("[DEBUG-NODE] WEBSOCKET_EVENT_DATA: op_code=0x%02x, len=%d\n",
                              data->op_code, data->data_len);
            }
            if (data->op_code == 0x01) {  // Text frame
                if (self->verbose_) {
                    Serial.printf("[DEBUG-NODE] Processing text frame: %d bytes\n", data->data_len);
                    // Print first 100 chars of message
                    char preview[101];
                    int preview_len = data->data_len < 100 ? data->data_len : 100;
                    memcpy(preview, data->data_ptr, preview_len);
                    preview[preview_len] = '\0';
                    Serial.printf("[DEBUG-NODE] Message preview: %s\n", preview);
                }
                
                self->handleMessage((const char*)data->data_ptr, data->data_len);
            } else {
                if (self->verbose_) {
                    Serial.printf("[DEBUG-NODE] Ignoring non-text frame: op_code=0x%02x\n", data->op_code);
                }
            }
            break;
            
        case WEBSOCKET_EVENT_ERROR:
            LW_LOGE("WS error");
            if (self->verbose_) Serial.printf("[DEBUG-NODE] WEBSOCKET_EVENT_ERROR\n");
            break;
            
        default:
            if (self->verbose_) Serial.printf("[DEBUG-NODE] Unknown WS event: %d\n", event_id);
            break;
    }
}

void NodeWsCtrl::sendHello() {
    String mac = WiFi.macAddress();
    
    JsonDocument doc;
    doc["t"] = "hello";
    doc["proto"] = LW_PROTO_VER;
    doc["mac"] = mac;
    doc["fw"] = "k1-v2.0.0";
    doc["caps"]["udp"] = true;
    doc["caps"]["ota"] = true;
    doc["caps"]["clock"] = true;
    doc["topo"]["leds"] = 320;
    doc["topo"]["channels"] = 2;
    
    String json;
    serializeJson(doc, json);
    
    if (esp_websocket_client_is_connected(ws_)) {
        esp_websocket_client_send_text(ws_, json.c_str(), json.length(), portMAX_DELAY);
        LW_LOGI("Sent HELLO");
    }
}

void NodeWsCtrl::sendKeepalive() {
    JsonDocument doc;
    doc["t"] = "ka";
    doc["nodeId"] = nodeId_;
    doc["token"] = token_;
    doc["rssi"] = WiFi.RSSI();
    doc["loss_pct"] = 0;
    doc["drift_us"] = 0;
    doc["uptime_s"] = millis() / 1000;
    
    String json;
    serializeJson(doc, json);
    
    if (esp_websocket_client_is_connected(ws_)) {
        esp_websocket_client_send_text(ws_, json.c_str(), json.length(), portMAX_DELAY);
    }
}

void NodeWsCtrl::sendOtaStatus(const char* state, uint8_t progress, const char* error) {
    if (!esp_websocket_client_is_connected(ws_)) return;
    
    JsonDocument doc;
    doc["t"] = "ota_status";
    doc["nodeId"] = nodeId_;
    doc["token"] = token_;
    doc["state"] = state;
    doc["pct"] = progress;
    if (error && error[0] != '\0') {
        doc["error"] = error;
    }
    
    String json;
    serializeJson(doc, json);
    
    esp_websocket_client_send_text(ws_, json.c_str(), json.length(), portMAX_DELAY);
    LW_LOGI("Sent OTA status: state=%s, progress=%d%%", state, progress);
}

void NodeWsCtrl::sendTsPing() {
    bool connected = esp_websocket_client_is_connected(ws_);
    LW_LOGD("sendTsPing: connected=%d, nodeId=%d", connected, nodeId_);
    
    if (!connected) {
        LW_LOGW("WS not connected, skipping ts_ping");
        return;
    }
    
    uint64_t t1_us = micros();
    
    JsonDocument doc;
    doc["t"] = "ts_ping";
    doc["nodeId"] = nodeId_;
    doc["token"] = token_;
    doc["seq"] = tsPingSeq_++;
    doc["t1_us"] = (unsigned long long)t1_us;
    
    String json;
    serializeJson(doc, json);
    
    esp_websocket_client_send_text(ws_, json.c_str(), json.length(), portMAX_DELAY);
}

uint64_t NodeWsCtrl::resolveApplyAtLocal(uint64_t applyAt_hub_us) const {
    // Best-effort scheduling: if time sync isn't locked yet, apply immediately.
    const uint64_t now_us = lw_monotonic_us();
    if (!timesync_ || !timesync_->isLocked()) {
        return now_us;
    }

    const int64_t applyAt_local_us = (int64_t)timesync_->hubToLocal(applyAt_hub_us);
    const int64_t delta_us = applyAt_local_us - (int64_t)now_us;

    // Guardrail: prevent pathological scheduling (queue fill / multi-second latency) if epochs drift.
    // For correct clocks, delta should be ~LW_APPLY_AHEAD_US (+/- jitter).
    if (delta_us > 500000 || delta_us < -500000) {
        LW_LOGW("applyAt out of range: hub=%llu local=%lld now=%llu delta=%lld offset=%lld (clamping)",
                (unsigned long long)applyAt_hub_us,
                (long long)applyAt_local_us,
                (unsigned long long)now_us,
                (long long)delta_us,
                timesync_ ? (long long)timesync_->getOffsetUs() : 0LL);
        return now_us + LW_APPLY_AHEAD_US;
    }

    return (uint64_t)applyAt_local_us;
}

void NodeWsCtrl::handleMessage(const char* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        LW_LOGE("JSON parse error: %s", error.c_str());
        return;
    }
    
    const char* msgType = nullptr;
    if (doc["t"].is<const char*>()) {
        msgType = doc["t"].as<const char*>();
    } else if (doc["type"].is<const char*>()) {
        msgType = doc["type"].as<const char*>();
    }
    if (!msgType) return;
    
    if (strcmp(msgType, "welcome") == 0) {
        nodeId_ = doc["nodeId"] | 0;
        strlcpy(token_, doc["token"] | "", sizeof(token_));
        tokenHash_ = lw_token_hash32(token_);
        
        state_ = WS_STATE_AUTHENTICATED;
        // Send an immediate keepalive after WELCOME so the hub can transition the node
        // from AUTHED->READY without waiting for the first periodic tick.
        lastKeepalive_ms_ = millis();
        lastTsPing_ms_ = millis();
        
        LW_LOGI("Received WELCOME: nodeId=%d, tokenHash=0x%08X", nodeId_, tokenHash_);

        // Kick the READY gate as soon as possible (hub timeout is ~3.5s).
        sendKeepalive();
        
        if (onWelcome) {
            onWelcome(nodeId_, token_);
        }
    }
    else if (strcmp(msgType, "ota_update") == 0) {
        const char* url = doc["url"] | "";
        const char* sha256 = doc["sha256"] | "";
        uint32_t size = doc["size"] | 0;
        
        LW_LOGI("Received OTA_UPDATE: url=%s, size=%u", url, size);
        
        if (ota_ && url[0] != '\0') {
            if (!ota_->startUpdate(url, sha256, size)) {
                LW_LOGE("Failed to start OTA update");
            }
        } else {
            LW_LOGW("OTA update requested but OTA client not available");
        }
    }
    else if (strcmp(msgType, "state.snapshot") == 0) {
        if (!scheduler_) return;

        const uint64_t applyAt_hub_us = doc["applyAt_us"] | 0ULL;
        const uint64_t applyAt_local_us = resolveApplyAtLocal(applyAt_hub_us);
        const bool zonesEnabled = doc["zonesEnabled"] | false;

        JsonObject global = doc["global"];
        if (!global.isNull()) {
            const uint8_t effectId = global["effectId"] | lastEffectId_;
            const uint8_t paletteId = global["paletteId"] | lastPaletteId_;

            lastEffectId_ = effectId;
            lastPaletteId_ = paletteId;

            lw_cmd_t scene = {};
            scene.type = LW_CMD_SCENE_CHANGE;
            scene.applyAt_us = applyAt_local_us;
            scene.trace_seq = ++wsCmdSeq_;
            scene.data.scene.effectId = effectId;
            scene.data.scene.paletteId = paletteId;
            scheduler_->enqueue(&scene);

            lw_cmd_t params = {};
            params.type = LW_CMD_PARAM_DELTA;
            params.applyAt_us = applyAt_local_us;
            params.trace_seq = ++wsCmdSeq_;
            params.data.params.flags = 0;

            if (global["brightness"].is<uint8_t>()) {
                params.data.params.brightness = global["brightness"].as<uint8_t>();
                params.data.params.flags |= LW_PARAM_F_BRIGHTNESS;
            }
            if (global["speed"].is<uint8_t>()) {
                params.data.params.speed = global["speed"].as<uint8_t>();
                params.data.params.flags |= LW_PARAM_F_SPEED;
            }
            if (global["paletteId"].is<uint8_t>()) {
                params.data.params.paletteId = global["paletteId"].as<uint8_t>();
                params.data.params.flags |= LW_PARAM_F_PALETTE;
            }
            if (global["hue"].is<uint8_t>()) {
                params.data.params.hue = (uint16_t)global["hue"].as<uint8_t>() << 8;
                params.data.params.flags |= LW_PARAM_F_HUE;
            }
            if (global["intensity"].is<uint8_t>()) {
                params.data.params.intensity = global["intensity"].as<uint8_t>();
                params.data.params.flags |= LW_PARAM_F_INTENSITY;
            }
            if (global["saturation"].is<uint8_t>()) {
                params.data.params.saturation = global["saturation"].as<uint8_t>();
                params.data.params.flags |= LW_PARAM_F_SATURATION;
            }
            if (global["complexity"].is<uint8_t>()) {
                params.data.params.complexity = global["complexity"].as<uint8_t>();
                params.data.params.flags |= LW_PARAM_F_COMPLEXITY;
            }
            if (global["variation"].is<uint8_t>()) {
                params.data.params.variation = global["variation"].as<uint8_t>();
                params.data.params.flags |= LW_PARAM_F_VARIATION;
            }

            if (params.data.params.flags != 0) {
                scheduler_->enqueue(&params);
            }
        }

        JsonArray zones = doc["zones"];
        if (zonesEnabled && !zones.isNull()) {
            if (!zoneComposer.isEnabled()) {
                zoneComposer.setEnabled(true);
                LW_LOGI("ZoneComposer ENABLED via state.snapshot (zonesEnabled=true)");
            }

            for (JsonObject z : zones) {
                if (z.isNull()) continue;
                const uint8_t zoneId = z["zoneId"] | 0;

                lw_cmd_t zu = {};
                zu.type = LW_CMD_ZONE_UPDATE;
                zu.applyAt_us = applyAt_local_us;
                zu.trace_seq = ++wsCmdSeq_;
                zu.data.zone.zoneId = zoneId;
                zu.data.zone.flags = 0;

                if (z["effectId"].is<uint8_t>()) {
                    zu.data.zone.effectId = z["effectId"].as<uint8_t>();
                    zu.data.zone.flags |= LW_ZONE_F_EFFECT;
                }
                if (z["brightness"].is<uint8_t>()) {
                    zu.data.zone.brightness = z["brightness"].as<uint8_t>();
                    zu.data.zone.flags |= LW_ZONE_F_BRIGHTNESS;
                }
                if (z["speed"].is<uint8_t>()) {
                    zu.data.zone.speed = z["speed"].as<uint8_t>();
                    zu.data.zone.flags |= LW_ZONE_F_SPEED;
                }
                if (z["paletteId"].is<uint8_t>()) {
                    zu.data.zone.paletteId = z["paletteId"].as<uint8_t>();
                    zu.data.zone.flags |= LW_ZONE_F_PALETTE;
                }
                if (z["blendMode"].is<uint8_t>()) {
                    zu.data.zone.blendMode = z["blendMode"].as<uint8_t>();
                    zu.data.zone.flags |= LW_ZONE_F_BLEND;
                }

                if (zu.data.zone.flags != 0) {
                    scheduler_->enqueue(&zu);
                }
            }
        }
    }
    else if (strcmp(msgType, "effects.setCurrent") == 0) {
        if (!scheduler_) return;
        const uint8_t effectId = doc["effectId"] | lastEffectId_;
        const uint64_t applyAt_hub_us = doc["applyAt_us"] | 0ULL;
        const uint64_t applyAt_local_us = resolveApplyAtLocal(applyAt_hub_us);

        lastEffectId_ = effectId;

        lw_cmd_t scene = {};
        scene.type = LW_CMD_SCENE_CHANGE;
        scene.applyAt_us = applyAt_local_us;
        scene.trace_seq = ++wsCmdSeq_;
        scene.data.scene.effectId = effectId;
        scene.data.scene.paletteId = lastPaletteId_;
        scheduler_->enqueue(&scene);
    }
    else if (strcmp(msgType, "parameters.set") == 0) {
        if (!scheduler_) return;
        const uint64_t applyAt_hub_us = doc["applyAt_us"] | 0ULL;
        const uint64_t applyAt_local_us = resolveApplyAtLocal(applyAt_hub_us);

        lw_cmd_t params = {};
        params.type = LW_CMD_PARAM_DELTA;
        params.applyAt_us = applyAt_local_us;
        params.trace_seq = ++wsCmdSeq_;
        params.data.params.flags = 0;

        if (doc["brightness"].is<uint8_t>()) {
            params.data.params.brightness = doc["brightness"].as<uint8_t>();
            params.data.params.flags |= LW_PARAM_F_BRIGHTNESS;
        }
        if (doc["speed"].is<uint8_t>()) {
            params.data.params.speed = doc["speed"].as<uint8_t>();
            params.data.params.flags |= LW_PARAM_F_SPEED;
        }
        if (doc["paletteId"].is<uint8_t>()) {
            const uint8_t pal = doc["paletteId"].as<uint8_t>();
            params.data.params.paletteId = pal;
            params.data.params.flags |= LW_PARAM_F_PALETTE;
            lastPaletteId_ = pal;
        }
        if (doc["hue"].is<uint8_t>()) {
            params.data.params.hue = (uint16_t)doc["hue"].as<uint8_t>() << 8;
            params.data.params.flags |= LW_PARAM_F_HUE;
        }
        if (doc["intensity"].is<uint8_t>()) {
            params.data.params.intensity = doc["intensity"].as<uint8_t>();
            params.data.params.flags |= LW_PARAM_F_INTENSITY;
        }
        if (doc["saturation"].is<uint8_t>()) {
            params.data.params.saturation = doc["saturation"].as<uint8_t>();
            params.data.params.flags |= LW_PARAM_F_SATURATION;
        }
        if (doc["complexity"].is<uint8_t>()) {
            params.data.params.complexity = doc["complexity"].as<uint8_t>();
            params.data.params.flags |= LW_PARAM_F_COMPLEXITY;
        }
        if (doc["variation"].is<uint8_t>()) {
            params.data.params.variation = doc["variation"].as<uint8_t>();
            params.data.params.flags |= LW_PARAM_F_VARIATION;
        }

        if (params.data.params.flags != 0) {
            scheduler_->enqueue(&params);
        }
    }
    else if (strcmp(msgType, "zones.update") == 0) {
        if (!scheduler_) return;

        const uint8_t zoneId = doc["zoneId"] | 0;
        const uint64_t applyAt_hub_us = doc["applyAt_us"] | 0ULL;
        const uint64_t applyAt_local_us = resolveApplyAtLocal(applyAt_hub_us);

        if (!zoneComposer.isEnabled()) {
            zoneComposer.setEnabled(true);
            LW_LOGI("ZoneComposer ENABLED via zones.update");
        }

        lw_cmd_t zu = {};
        zu.type = LW_CMD_ZONE_UPDATE;
        zu.applyAt_us = applyAt_local_us;
        zu.trace_seq = ++wsCmdSeq_;
        zu.data.zone.zoneId = zoneId;
        zu.data.zone.flags = 0;

        if (doc["effectId"].is<uint8_t>()) {
            zu.data.zone.effectId = doc["effectId"].as<uint8_t>();
            zu.data.zone.flags |= LW_ZONE_F_EFFECT;
        }
        if (doc["brightness"].is<uint8_t>()) {
            zu.data.zone.brightness = doc["brightness"].as<uint8_t>();
            zu.data.zone.flags |= LW_ZONE_F_BRIGHTNESS;
        }
        if (doc["speed"].is<uint8_t>()) {
            zu.data.zone.speed = doc["speed"].as<uint8_t>();
            zu.data.zone.flags |= LW_ZONE_F_SPEED;
        }
        if (doc["paletteId"].is<uint8_t>()) {
            zu.data.zone.paletteId = doc["paletteId"].as<uint8_t>();
            zu.data.zone.flags |= LW_ZONE_F_PALETTE;
        }
        if (doc["blendMode"].is<uint8_t>()) {
            zu.data.zone.blendMode = doc["blendMode"].as<uint8_t>();
            zu.data.zone.flags |= LW_ZONE_F_BLEND;
        }

        if (zu.data.zone.flags != 0) {
            scheduler_->enqueue(&zu);
        }
    }
    // WS ts_pong handling removed - now using dedicated UDP time-sync
}
