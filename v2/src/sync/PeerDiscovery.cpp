/**
 * @file PeerDiscovery.cpp
 * @brief mDNS peer discovery implementation
 */

#include "PeerDiscovery.h"
#include <cstring>

#ifdef NATIVE_BUILD
// Native build - mock mDNS
namespace {
    // Mock functions for native testing
    int mock_mdns_init() { return 0; }
    int mock_mdns_query_srv(const char*, const char*, int) { return 0; }
    const char* mock_mdns_result_ip(int) { return "192.168.1.100"; }
    uint16_t mock_mdns_result_port(int) { return 80; }
    const char* mock_mdns_result_hostname(int) { return "lightwave-test"; }
    const char* mock_mdns_result_txt(int, const char*) { return nullptr; }
}
#define MDNS_init() mock_mdns_init()
#define MDNS_queryService(t, p, timeout) mock_mdns_query_srv(t, p, timeout)
#else
// ESP32 build - use real mDNS
#include <ESPmDNS.h>
#define MDNS_init() MDNS.begin("lightwaveos")
#define MDNS_queryService(t, p, timeout) MDNS.queryService(t, p)
#endif

#ifndef millis
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace sync {

PeerDiscovery::PeerDiscovery()
    : m_peers{}
    , m_peerCount(0)
    , m_lastScanMs(0)
    , m_scanning(false)
    , m_initialized(false)
    , m_callback(nullptr)
{
}

bool PeerDiscovery::begin() {
    if (m_initialized) return true;

#ifdef NATIVE_BUILD
    m_initialized = true;
    return true;
#else
    // Note: MDNS.begin() should already be called by WebServer
    // We just verify it's ready
    m_initialized = true;
    return true;
#endif
}

void PeerDiscovery::scan() {
    if (!m_initialized || m_scanning) return;

    m_scanning = true;
    m_lastScanMs = millis();

#ifdef NATIVE_BUILD
    // Native build: No actual mDNS, just mark as complete
    m_scanning = false;
#else
    // ESP32: Query for WebSocket services
    // This is a blocking call, typically takes 500ms-2s
    int count = MDNS.queryService(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO);
    processScanResults(count);
    m_scanning = false;
#endif
}

void PeerDiscovery::processScanResults(int resultCount) {
#ifdef NATIVE_BUILD
    (void)resultCount;
    return;
#else
    for (int i = 0; i < resultCount; i++) {
        // Get TXT record for board type
        const char* board = MDNS.txt(i, MDNS_TXT_BOARD);
        if (!board || strcmp(board, MDNS_TXT_BOARD_VALUE) != 0) {
            continue; // Not a LightwaveOS device
        }

        // Get UUID from TXT record
        const char* uuid = MDNS.txt(i, MDNS_TXT_UUID);
        if (!uuid || strlen(uuid) != 15) {
            continue; // Invalid UUID
        }

        // Skip self
        if (DEVICE_UUID.matches(uuid)) {
            continue;
        }

        // Build PeerInfo
        PeerInfo peer;
        strncpy(peer.uuid, uuid, sizeof(peer.uuid) - 1);
        peer.uuid[sizeof(peer.uuid) - 1] = '\0';

        String hostname = MDNS.hostname(i);
        strncpy(peer.hostname, hostname.c_str(), sizeof(peer.hostname) - 1);
        peer.hostname[sizeof(peer.hostname) - 1] = '\0';

        IPAddress ip = MDNS.IP(i);
        peer.ip[0] = ip[0];
        peer.ip[1] = ip[1];
        peer.ip[2] = ip[2];
        peer.ip[3] = ip[3];

        peer.port = MDNS.port(i);
        peer.lastSeenMs = millis();
        peer.role = SyncRole::UNKNOWN;
        peer.connected = false;

        addOrUpdatePeer(peer);
    }
#endif
}

bool PeerDiscovery::addOrUpdatePeer(const PeerInfo& peer) {
    // Check if peer already exists
    for (uint8_t i = 0; i < m_peerCount; i++) {
        if (strcmp(m_peers[i].uuid, peer.uuid) == 0) {
            // Update existing peer
            m_peers[i].ip[0] = peer.ip[0];
            m_peers[i].ip[1] = peer.ip[1];
            m_peers[i].ip[2] = peer.ip[2];
            m_peers[i].ip[3] = peer.ip[3];
            m_peers[i].port = peer.port;
            m_peers[i].lastSeenMs = peer.lastSeenMs;
            // Don't overwrite hostname, role, or connected status
            return false; // Updated, not added
        }
    }

    // Add new peer if room
    if (m_peerCount < MAX_DISCOVERED_PEERS) {
        m_peers[m_peerCount] = peer;
        m_peerCount++;

        if (m_callback) {
            m_callback(peer, true);
        }
        return true; // Added
    }

    return false; // No room
}

void PeerDiscovery::removePeerAt(uint8_t index) {
    if (index >= m_peerCount) return;

    PeerInfo removed = m_peers[index];

    // Shift remaining peers down
    for (uint8_t i = index; i < m_peerCount - 1; i++) {
        m_peers[i] = m_peers[i + 1];
    }
    m_peerCount--;

    // Clear the now-unused slot
    m_peers[m_peerCount] = PeerInfo();

    if (m_callback) {
        m_callback(removed, false);
    }
}

uint8_t PeerDiscovery::update(uint32_t nowMs) {
    uint8_t removed = 0;

    // Iterate backwards to safely remove stale peers
    for (int i = static_cast<int>(m_peerCount) - 1; i >= 0; i--) {
        if (m_peers[i].isStale(nowMs)) {
            removePeerAt(static_cast<uint8_t>(i));
            removed++;
        }
    }

    return removed;
}

const PeerInfo* PeerDiscovery::findPeer(const char* uuid) const {
    if (!uuid) return nullptr;

    for (uint8_t i = 0; i < m_peerCount; i++) {
        if (strcmp(m_peers[i].uuid, uuid) == 0) {
            return &m_peers[i];
        }
    }
    return nullptr;
}

bool PeerDiscovery::touchPeer(const char* uuid, uint32_t nowMs) {
    for (uint8_t i = 0; i < m_peerCount; i++) {
        if (strcmp(m_peers[i].uuid, uuid) == 0) {
            m_peers[i].lastSeenMs = nowMs;
            return true;
        }
    }
    return false;
}

bool PeerDiscovery::setPeerConnected(const char* uuid, bool connected) {
    for (uint8_t i = 0; i < m_peerCount; i++) {
        if (strcmp(m_peers[i].uuid, uuid) == 0) {
            m_peers[i].connected = connected;
            return true;
        }
    }
    return false;
}

uint32_t PeerDiscovery::getTimeUntilNextScan(uint32_t nowMs) const {
    uint32_t elapsed = nowMs - m_lastScanMs;
    if (elapsed >= PEER_SCAN_INTERVAL_MS) {
        return 0; // Time to scan
    }
    return PEER_SCAN_INTERVAL_MS - elapsed;
}

} // namespace sync
} // namespace lightwaveos
