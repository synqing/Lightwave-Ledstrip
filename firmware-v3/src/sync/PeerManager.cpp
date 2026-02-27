// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file PeerManager.cpp
 * @brief WebSocket client connection management implementation using esp_websocket_client
 *
 * Uses ESP-IDF native esp_websocket_client for non-blocking WebSocket connections.
 * Each connection runs its own FreeRTOS task internally, and events are routed
 * back to PeerManager via a static callback.
 */

#include "PeerManager.h"
#include <cstring>
#include <cstdio>

#ifdef NATIVE_BUILD
// Native build - mock implementation
#define millis() 0
// Mock client structure for native testing
struct NativeMockClient {
    bool connected = false;
    void close() { connected = false; }
    void text(const char*) {}
};
static NativeMockClient* s_mockClients[4] = {nullptr};  // Mock storage
#else
// IMPORTANT: Include Arduino/WiFi headers BEFORE esp_websocket_client.h
// to avoid INADDR_NONE macro collision with lwIP headers
#include <Arduino.h>
#include <WiFi.h>

// Undefine conflicting macros from lwIP before including esp_websocket_client
#ifdef INADDR_NONE
#undef INADDR_NONE
#endif

#include "esp_websocket_client.h"
#include "esp_log.h"

static const char* TAG = "PeerManager";
#endif

namespace lightwaveos {
namespace sync {

// ============================================================================
// Static Event Handler (Member Function)
// ============================================================================

#ifndef NATIVE_BUILD
/**
 * @brief Static WebSocket event handler - routes events to PeerManager instance
 *
 * This is registered with esp_websocket_register_events() and receives all
 * WebSocket events for a connection. The handler_args pointer is the PeerManager
 * instance, and we find the specific PeerConnection via the client handle.
 */
void PeerManager::wsEventHandler(void* handler_args, esp_event_base_t base,
                                  int32_t event_id, void* event_data) {
    PeerManager* self = static_cast<PeerManager*>(handler_args);
    esp_websocket_event_data_t* data = static_cast<esp_websocket_event_data_t*>(event_data);

    if (!self || !data) return;

    // Find the connection associated with this client handle
    PeerConnection* conn = self->findSlotByHandle(data->client);
    if (!conn) {
        ESP_LOGW(TAG, "Event for unknown client handle");
        return;
    }

    // Dispatch to instance method
    self->handleWebSocketEvent(event_id, event_data, conn);
}

/**
 * @brief Instance method to handle WebSocket events
 */
void PeerManager::handleWebSocketEvent(int32_t event_id, void* event_data, PeerConnection* conn) {
    esp_websocket_event_data_t* data = static_cast<esp_websocket_event_data_t*>(event_data);

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to peer %s", conn->uuid);
            onConnect(conn);
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from peer %s", conn->uuid);
            onDisconnect(conn);
            break;

        case WEBSOCKET_EVENT_DATA:
            // Handle incoming message
            if (data->op_code == 0x01 && data->data_len > 0) {
                // Text frame - pass to message handler
                onMessage(conn, data->data_ptr, data->data_len);
            } else if (data->op_code == 0x0A) {
                // Pong frame - update activity
                conn->lastActivityMs = millis();
                conn->missedPings = 0;
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error for peer %s", conn->uuid);
            onDisconnect(conn);
            break;

        case WEBSOCKET_EVENT_CLOSED:
            ESP_LOGI(TAG, "WebSocket closed for peer %s", conn->uuid);
            onDisconnect(conn);
            break;

        default:
            break;
    }
}
#endif

// ============================================================================
// PeerManager Implementation
// ============================================================================

PeerManager::PeerManager()
    : m_connections{}
    , m_messageCallback(nullptr)
    , m_connectionCallback(nullptr)
    , m_initialized(false)
{
}

PeerManager::~PeerManager() {
    disconnectAll();
}

void PeerManager::begin() {
    m_initialized = true;
#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "PeerManager initialized");
#endif
}

void PeerManager::update(uint32_t nowMs) {
    if (!m_initialized) return;

    // Send heartbeats to connected peers
    sendHeartbeats(nowMs);

    // Check for stale connections (missed heartbeats)
    checkHeartbeats(nowMs);

    // Note: esp_websocket_client handles its own event loop internally,
    // so we don't need to call loop() on each connection.
}

bool PeerManager::connectToPeer(const PeerInfo& peer) {
    if (!m_initialized) return false;

    // Check if already connected or connecting
    PeerConnection* existing = findSlot(peer.uuid);
    if (existing) {
        return existing->connected || existing->connecting;
    }

    // Find empty slot
    PeerConnection* slot = findEmptySlot();
    if (!slot) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "No connection slots available");
#endif
        return false;
    }

    // Initialize the slot
    strncpy(slot->uuid, peer.uuid, sizeof(slot->uuid) - 1);
    slot->uuid[sizeof(slot->uuid) - 1] = '\0';
    slot->ip[0] = peer.ip[0];
    slot->ip[1] = peer.ip[1];
    slot->ip[2] = peer.ip[2];
    slot->ip[3] = peer.ip[3];
    slot->port = peer.port;
    slot->connecting = true;
    slot->connected = false;
    slot->lastActivityMs = millis();
    slot->reconnectDelayMs = RECONNECT_INITIAL_MS;
    slot->missedPings = 0;

#ifdef NATIVE_BUILD
    // Native build: Simulate immediate connection with mock handle
    // Find an available mock slot
    int mockIdx = -1;
    for (int i = 0; i < 4; i++) {
        if (s_mockClients[i] == nullptr) {
            mockIdx = i;
            break;
        }
    }
    if (mockIdx >= 0) {
        s_mockClients[mockIdx] = new NativeMockClient();
        s_mockClients[mockIdx]->connected = true;
        slot->client = reinterpret_cast<esp_websocket_client_handle_t>(s_mockClients[mockIdx]);
    }
    slot->connecting = false;
    slot->connected = true;
    onConnect(slot);
#else
    // ESP32: Create WebSocket client using esp_websocket_client

    // Format URI: "ws://192.168.1.100:80/ws"
    char uri[64];
    snprintf(uri, sizeof(uri), "ws://%d.%d.%d.%d:%d/ws",
             slot->ip[0], slot->ip[1], slot->ip[2], slot->ip[3], slot->port);

    ESP_LOGI(TAG, "Connecting to peer %s at %s", slot->uuid, uri);

    // Configure the WebSocket client
    esp_websocket_client_config_t config = {};
    config.uri = uri;
    config.disable_auto_reconnect = true;  // We handle reconnect with backoff
    config.task_prio = 5;                  // Lower than render task
    config.task_stack = 4096;              // 4KB stack for WS task
    config.buffer_size = 2048;             // Receive buffer
    config.ping_interval_sec = 0;          // We handle pings ourselves
    config.user_context = static_cast<void*>(slot);  // Store slot pointer

    // Create client
    esp_websocket_client_handle_t client = esp_websocket_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to create WebSocket client for %s", slot->uuid);
        slot->reset();
        return false;
    }

    // Store handle in slot
    slot->client = client;

    // Register event handler with PeerManager instance as context
    esp_err_t err = esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY,
                                                   wsEventHandler, this);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register events for %s: %s",
                 slot->uuid, esp_err_to_name(err));
        esp_websocket_client_destroy(client);
        slot->reset();
        return false;
    }

    // Start the connection (non-blocking)
    err = esp_websocket_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket client for %s: %s",
                 slot->uuid, esp_err_to_name(err));
        esp_websocket_client_destroy(client);
        slot->reset();
        return false;
    }

    ESP_LOGI(TAG, "Connection initiated to peer %s", slot->uuid);
#endif

    return true;
}

void PeerManager::disconnectPeer(const char* uuid) {
    PeerConnection* slot = findSlot(uuid);
    if (!slot) return;

#ifdef NATIVE_BUILD
    if (slot->client) {
        // Find and clean up the mock client
        NativeMockClient* mock = reinterpret_cast<NativeMockClient*>(slot->client);
        for (int i = 0; i < 4; i++) {
            if (s_mockClients[i] == mock) {
                mock->close();
                delete mock;
                s_mockClients[i] = nullptr;
                break;
            }
        }
        slot->client = nullptr;
    }
#else
    // Close and destroy the esp_websocket_client
    if (slot->client) {
        ESP_LOGI(TAG, "Disconnecting from peer %s", uuid);

        // Close with timeout (5 seconds)
        esp_websocket_client_close(slot->client, pdMS_TO_TICKS(5000));

        // Destroy the client handle
        esp_websocket_client_destroy(slot->client);
        slot->client = nullptr;
    }
#endif

    bool wasConnected = slot->connected;
    slot->reset();

    if (wasConnected && m_connectionCallback) {
        m_connectionCallback(uuid, false);
    }
}

void PeerManager::disconnectAll() {
    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        if (m_connections[i].inUse()) {
            disconnectPeer(m_connections[i].uuid);
        }
    }
}

bool PeerManager::sendTo(const char* uuid, const char* message) {
    if (!message) return false;

    PeerConnection* slot = findSlot(uuid);
    if (!slot || !slot->connected) return false;

#ifdef NATIVE_BUILD
    if (slot->client) {
        NativeMockClient* mock = reinterpret_cast<NativeMockClient*>(slot->client);
        mock->text(message);
    }
#else
    if (slot->client && esp_websocket_client_is_connected(slot->client)) {
        size_t len = strlen(message);
        int sent = esp_websocket_client_send_text(slot->client, message, len,
                                                   pdMS_TO_TICKS(1000));
        if (sent < 0) {
            ESP_LOGW(TAG, "Failed to send message to %s", uuid);
            return false;
        }
    } else {
        return false;
    }
#endif

    slot->lastActivityMs = millis();
    return true;
}

uint8_t PeerManager::broadcast(const char* message) {
    if (!message) return 0;

    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        PeerConnection& conn = m_connections[i];
        if (conn.connected) {
            if (sendTo(conn.uuid, message)) {
                count++;
            }
        }
    }
    return count;
}

uint8_t PeerManager::getConnectedCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        if (m_connections[i].connected) {
            count++;
        }
    }
    return count;
}

uint8_t PeerManager::getActiveSlotCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        if (m_connections[i].inUse()) {
            count++;
        }
    }
    return count;
}

bool PeerManager::isConnectedTo(const char* uuid) const {
    const PeerConnection* slot = findSlot(uuid);
    return slot && slot->connected;
}

uint8_t PeerManager::getConnectedPeerUuids(char (*outUuids)[16], uint8_t maxCount) const {
    if (!outUuids || maxCount == 0) return 0;

    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS && count < maxCount; i++) {
        if (m_connections[i].connected) {
            strncpy(outUuids[count], m_connections[i].uuid, 15);
            outUuids[count][15] = '\0';
            count++;
        }
    }
    return count;
}

PeerConnection* PeerManager::findSlot(const char* uuid) {
    if (!uuid) return nullptr;

    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        if (m_connections[i].inUse() &&
            strcmp(m_connections[i].uuid, uuid) == 0) {
            return &m_connections[i];
        }
    }
    return nullptr;
}

const PeerConnection* PeerManager::findSlot(const char* uuid) const {
    if (!uuid) return nullptr;

    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        if (m_connections[i].inUse() &&
            strcmp(m_connections[i].uuid, uuid) == 0) {
            return &m_connections[i];
        }
    }
    return nullptr;
}

PeerConnection* PeerManager::findEmptySlot() {
    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        if (!m_connections[i].inUse()) {
            return &m_connections[i];
        }
    }
    return nullptr;
}

PeerConnection* PeerManager::findSlotByHandle(esp_websocket_client_handle_t handle) {
    if (!handle) return nullptr;

    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        if (m_connections[i].inUse() && m_connections[i].client == handle) {
            return &m_connections[i];
        }
    }
    return nullptr;
}

void PeerManager::onConnect(PeerConnection* conn) {
    if (!conn) return;

    conn->connecting = false;
    conn->connected = true;
    conn->lastActivityMs = millis();
    conn->reconnectDelayMs = RECONNECT_INITIAL_MS; // Reset backoff
    conn->missedPings = 0;

    if (m_connectionCallback) {
        m_connectionCallback(conn->uuid, true);
    }
}

void PeerManager::onDisconnect(PeerConnection* conn) {
    if (!conn) return;

    bool wasConnected = conn->connected;
    conn->connected = false;
    conn->connecting = false;

    // Apply exponential backoff for reconnection
    conn->reconnectDelayMs *= 2;
    if (conn->reconnectDelayMs > RECONNECT_MAX_MS) {
        conn->reconnectDelayMs = RECONNECT_MAX_MS;
    }

    // Note: Don't call close/destroy here as we may be in the event callback.
    // The handle should be cleaned up via disconnectPeer() or on reconnect.

    if (wasConnected && m_connectionCallback) {
        m_connectionCallback(conn->uuid, false);
    }
}

void PeerManager::onMessage(PeerConnection* conn, const char* message, size_t length) {
    if (!conn || !message) return;

    conn->lastActivityMs = millis();
    conn->missedPings = 0; // Any message counts as heartbeat

    if (m_messageCallback) {
        m_messageCallback(conn->uuid, message, length);
    }
}

void PeerManager::sendHeartbeats(uint32_t nowMs) {
    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        PeerConnection& conn = m_connections[i];
        if (!conn.connected) continue;

        uint32_t elapsed = nowMs - conn.lastPingMs;
        if (elapsed >= HEARTBEAT_INTERVAL_MS) {
            // Send ping message
            sendTo(conn.uuid, "{\"t\":\"sync.ping\"}");
            conn.lastPingMs = nowMs;
            conn.missedPings++;
        }
    }
}

void PeerManager::checkHeartbeats(uint32_t nowMs) {
    (void)nowMs;

    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        PeerConnection& conn = m_connections[i];
        if (!conn.connected) continue;

        if (conn.missedPings >= HEARTBEAT_MISS_LIMIT) {
            // Too many missed pings, disconnect
#ifndef NATIVE_BUILD
            ESP_LOGW(TAG, "Peer %s missed %d heartbeats, disconnecting",
                     conn.uuid, conn.missedPings);
#endif
            onDisconnect(&conn);
        }
    }
}

void PeerManager::attemptReconnects(uint32_t nowMs) {
    (void)nowMs;
    // Reconnection logic is called from SyncManagerActor
    // based on PeerDiscovery results and connection state.
    // The SyncManagerActor will call connectToPeer() again
    // after the reconnect delay has elapsed.
}

} // namespace sync
} // namespace lightwaveos
