/**
 * @file PeerManager.cpp
 * @brief WebSocket client connection management implementation
 */

#include "PeerManager.h"
#include <cstring>
#include <cstdio>

#ifdef NATIVE_BUILD
// Native build - mock WebSocket client
struct MockAsyncWebSocketClient {
    bool connected = false;
    void close() { connected = false; }
    void text(const char*) {}
};
#define millis() 0
#else
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#endif

namespace lightwaveos {
namespace sync {

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
}

void PeerManager::update(uint32_t nowMs) {
    if (!m_initialized) return;

#ifndef NATIVE_BUILD
    // Process WebSocket events for all connections
    for (uint8_t i = 0; i < MAX_PEER_CONNECTIONS; i++) {
        PeerConnection& conn = m_connections[i];
        if (!conn.inUse()) continue;

        // WebSocketsClient loop is handled internally
    }
#endif

    // Send heartbeats
    sendHeartbeats(nowMs);

    // Check for stale connections
    checkHeartbeats(nowMs);
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
        return false; // No slots available
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
    // Native build: Simulate immediate connection
    slot->client = new MockAsyncWebSocketClient();
    slot->client->connected = true;
    slot->connecting = false;
    slot->connected = true;
    onConnect(slot);
#else
    // ESP32: Create WebSocket client
    // Note: In a real implementation, we'd use WebSocketsClient
    // For now, this is a placeholder showing the connection flow

    // Format: "ws://192.168.1.100:80/ws"
    char url[64];
    snprintf(url, sizeof(url), "ws://%d.%d.%d.%d:%d/ws",
             slot->ip[0], slot->ip[1], slot->ip[2], slot->ip[3], slot->port);

    // TODO: Create actual WebSocket client connection
    // The ESP32 WebSocketsClient library doesn't directly support
    // multiple simultaneous client connections out of the box.
    // We may need to use the lower-level AsyncWebSocket as client
    // or create a connection pool.

    slot->connecting = false;
    slot->connected = false;

    // For now, mark as failed until we implement actual WS client
#endif

    return slot->connected || slot->connecting;
}

void PeerManager::disconnectPeer(const char* uuid) {
    PeerConnection* slot = findSlot(uuid);
    if (!slot) return;

#ifdef NATIVE_BUILD
    if (slot->client) {
        slot->client->close();
        delete slot->client;
    }
#else
    // Close WebSocket connection
    // TODO: Implement actual close
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
        slot->client->text(message);
    }
#else
    // TODO: Send via WebSocket client
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

    // Apply exponential backoff
    conn->reconnectDelayMs *= 2;
    if (conn->reconnectDelayMs > RECONNECT_MAX_MS) {
        conn->reconnectDelayMs = RECONNECT_MAX_MS;
    }

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
            // Send ping
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
            onDisconnect(&conn);
        }
    }
}

void PeerManager::attemptReconnects(uint32_t nowMs) {
    (void)nowMs;
    // Reconnection logic would be called from SyncManagerActor
    // based on PeerDiscovery results
}

} // namespace sync
} // namespace lightwaveos
