/**
 * @file PeerDiscovery.h
 * @brief mDNS-based peer discovery for multi-device sync
 *
 * Discovers other LightwaveOS devices on the local network using mDNS
 * browsing. Filters by TXT record to ensure only compatible devices
 * are discovered.
 *
 * Discovery Flow:
 * 1. scan() triggers mDNS query for _ws._tcp services
 * 2. Discovered devices are filtered by TXT record (board=ESP32-S3)
 * 3. Devices with valid UUID TXT records are added to peer list
 * 4. Stale peers (no activity > 90s) are automatically removed
 *
 * Threading:
 * - scan() should be called from SyncManagerActor
 * - update() should be called periodically to clean stale peers
 * - getPeers() returns a snapshot safe for concurrent read
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "SyncProtocol.h"
#include "DeviceUUID.h"
#include <cstdint>

namespace lightwaveos {
namespace sync {

/**
 * @brief Callback for peer discovery events
 *
 * Called when a new peer is discovered or when a peer is removed.
 */
using PeerDiscoveryCallback = void (*)(const PeerInfo& peer, bool added);

/**
 * @brief mDNS peer discovery manager
 *
 * Manages discovery of other LightwaveOS devices on the network.
 * Uses ESP32's mDNS library for zero-configuration networking.
 */
class PeerDiscovery {
public:
    PeerDiscovery();
    ~PeerDiscovery() = default;

    // Prevent copying
    PeerDiscovery(const PeerDiscovery&) = delete;
    PeerDiscovery& operator=(const PeerDiscovery&) = delete;

    /**
     * @brief Initialize mDNS discovery
     *
     * Must be called after WiFi is connected.
     *
     * @return true if mDNS initialized successfully
     */
    bool begin();

    /**
     * @brief Trigger an mDNS scan for peers
     *
     * Non-blocking. Results will be available in getPeers() after
     * the scan completes. Typical scan time is 500ms-2s.
     */
    void scan();

    /**
     * @brief Periodic update - clean stale peers
     *
     * Should be called periodically (e.g., every 10 seconds) to
     * remove peers that haven't been seen recently.
     *
     * @param nowMs Current time in milliseconds
     * @return Number of peers removed
     */
    uint8_t update(uint32_t nowMs);

    /**
     * @brief Get list of discovered peers
     *
     * Returns a pointer to the internal peer array. The array is
     * MAX_DISCOVERED_PEERS in size, but only peerCount() entries
     * are valid.
     *
     * Thread safety: Safe to read while scan() runs, but entries
     * may change between calls.
     *
     * @return Pointer to peer array
     */
    const PeerInfo* getPeers() const { return m_peers; }

    /**
     * @brief Get number of valid peers
     * @return Number of discovered peers (0-MAX_DISCOVERED_PEERS)
     */
    uint8_t peerCount() const { return m_peerCount; }

    /**
     * @brief Find peer by UUID
     * @param uuid UUID string ("LW-XXXXXXXXXXXX")
     * @return Pointer to PeerInfo or nullptr if not found
     */
    const PeerInfo* findPeer(const char* uuid) const;

    /**
     * @brief Update peer's last seen timestamp
     *
     * Called when we receive any message from a peer to reset
     * their stale timer.
     *
     * @param uuid UUID of the peer
     * @param nowMs Current time
     * @return true if peer was found and updated
     */
    bool touchPeer(const char* uuid, uint32_t nowMs);

    /**
     * @brief Update peer's connection status
     * @param uuid UUID of the peer
     * @param connected Whether connected as WS client
     * @return true if peer was found and updated
     */
    bool setPeerConnected(const char* uuid, bool connected);

    /**
     * @brief Register callback for discovery events
     * @param callback Function to call on peer add/remove
     */
    void setCallback(PeerDiscoveryCallback callback) {
        m_callback = callback;
    }

    /**
     * @brief Get time until next scheduled scan
     * @param nowMs Current time
     * @return Milliseconds until next scan (0 = scan now)
     */
    uint32_t getTimeUntilNextScan(uint32_t nowMs) const;

    /**
     * @brief Check if currently scanning
     */
    bool isScanning() const { return m_scanning; }

private:
    /**
     * @brief Add or update a peer from mDNS result
     * @return true if peer was added (not updated)
     */
    bool addOrUpdatePeer(const PeerInfo& peer);

    /**
     * @brief Remove a peer by index
     */
    void removePeerAt(uint8_t index);

    /**
     * @brief Process mDNS scan results
     * @param resultCount Number of services found
     */
    void processScanResults(int resultCount);

    PeerInfo m_peers[MAX_DISCOVERED_PEERS];     // Discovered peers
    uint8_t m_peerCount;                        // Number of valid peers
    uint32_t m_lastScanMs;                      // Last scan timestamp
    bool m_scanning;                            // Currently scanning
    bool m_initialized;                         // mDNS initialized
    PeerDiscoveryCallback m_callback;           // Discovery callback
};

} // namespace sync
} // namespace lightwaveos
