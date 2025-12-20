#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

/**
 * @file ConnectionManager.h
 * @brief WebSocket connection tracking and management for LightwaveOS
 *
 * Tracks active WebSocket connections with:
 * - Maximum concurrent connection enforcement
 * - Per-IP connection limits (prevent single client monopolizing slots)
 * - Idle connection detection and cleanup
 * - Activity tracking for each connection
 *
 * RAM Cost: ~128 bytes (4 connections * ~32 bytes each)
 */

#include <Arduino.h>
#include <WiFi.h>

class ConnectionManager {
public:
    // Configuration constants
    static constexpr uint8_t MAX_WS_CLIENTS = 4;          // Total WebSocket connections allowed
    static constexpr uint8_t MAX_CONNECTIONS_PER_IP = 2;  // Max connections from single IP
    static constexpr uint32_t IDLE_TIMEOUT_MS = 300000;   // 5 minutes idle timeout

    /**
     * @brief Per-connection tracking entry
     */
    struct Connection {
        IPAddress ip;           // Client IP address
        uint32_t clientId;      // WebSocket client ID
        uint32_t lastActivity;  // Last activity timestamp (millis)
        bool active;            // Is this slot in use?
    };

    ConnectionManager() {
        memset(connections, 0, sizeof(connections));
    }

    /**
     * @brief Check if a new connection from this IP can be accepted
     * @param ip Client IP address
     * @return true if connection allowed, false if would exceed limits
     */
    bool canAcceptConnection(IPAddress ip) {
        uint8_t activeCount = 0;
        uint8_t ipCount = 0;

        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].active) {
                activeCount++;
                if (connections[i].ip == ip) {
                    ipCount++;
                }
            }
        }

        // Check global limit
        if (activeCount >= MAX_WS_CLIENTS) {
            return false;
        }

        // Check per-IP limit
        if (ipCount >= MAX_CONNECTIONS_PER_IP) {
            return false;
        }

        return true;
    }

    /**
     * @brief Register a new WebSocket connection
     * @param ip Client IP address
     * @param clientId WebSocket client ID
     * @return true if registered successfully, false if no slot available
     */
    bool onConnect(IPAddress ip, uint32_t clientId) {
        // Find empty slot
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (!connections[i].active) {
                connections[i].ip = ip;
                connections[i].clientId = clientId;
                connections[i].lastActivity = millis();
                connections[i].active = true;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Unregister a WebSocket connection
     * @param clientId WebSocket client ID to remove
     */
    void onDisconnect(uint32_t clientId) {
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].clientId == clientId) {
                connections[i].active = false;
                connections[i].ip = IPAddress(0, 0, 0, 0);
                connections[i].clientId = 0;
                return;
            }
        }
    }

    /**
     * @brief Record activity for a connection (resets idle timer)
     * @param clientId WebSocket client ID
     */
    void onActivity(uint32_t clientId) {
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].active && connections[i].clientId == clientId) {
                connections[i].lastActivity = millis();
                return;
            }
        }
    }

    /**
     * @brief Find connections that have been idle too long
     * @param idleClients Output array for idle client IDs
     * @param count Output: number of idle clients found
     * @param maxResults Maximum number of results to return
     */
    void checkIdleConnections(uint32_t* idleClients, uint8_t& count, uint8_t maxResults = MAX_WS_CLIENTS) {
        count = 0;
        uint32_t now = millis();

        for (uint8_t i = 0; i < MAX_WS_CLIENTS && count < maxResults; i++) {
            if (connections[i].active) {
                if (now - connections[i].lastActivity > IDLE_TIMEOUT_MS) {
                    idleClients[count++] = connections[i].clientId;
                }
            }
        }
    }

    /**
     * @brief Get number of active connections
     * @return Number of active WebSocket connections
     */
    uint8_t getActiveCount() {
        uint8_t count = 0;
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].active) {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief Get number of connections from a specific IP
     * @param ip Client IP address
     * @return Number of connections from this IP
     */
    uint8_t getConnectionsFromIP(IPAddress ip) {
        uint8_t count = 0;
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].active && connections[i].ip == ip) {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief Check if a specific client ID is connected
     * @param clientId WebSocket client ID
     * @return true if connected, false otherwise
     */
    bool isConnected(uint32_t clientId) {
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].active && connections[i].clientId == clientId) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get connection info by client ID
     * @param clientId WebSocket client ID
     * @param ip Output: Client IP address
     * @param lastActivity Output: Last activity timestamp
     * @return true if found, false if not connected
     */
    bool getConnectionInfo(uint32_t clientId, IPAddress& ip, uint32_t& lastActivity) {
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].active && connections[i].clientId == clientId) {
                ip = connections[i].ip;
                lastActivity = connections[i].lastActivity;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get time since last activity for a connection
     * @param clientId WebSocket client ID
     * @return Milliseconds since last activity, or UINT32_MAX if not found
     */
    uint32_t getIdleTime(uint32_t clientId) {
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].active && connections[i].clientId == clientId) {
                return millis() - connections[i].lastActivity;
            }
        }
        return UINT32_MAX;
    }

    /**
     * @brief Disconnect all connections from a specific IP
     * @param ip IP address to disconnect
     * @param disconnectedClients Output array for disconnected client IDs
     * @param count Output: number of disconnected clients
     */
    void disconnectIP(IPAddress ip, uint32_t* disconnectedClients, uint8_t& count) {
        count = 0;
        for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
            if (connections[i].active && connections[i].ip == ip) {
                disconnectedClients[count++] = connections[i].clientId;
                connections[i].active = false;
            }
        }
    }

    /**
     * @brief Clear all connections
     */
    void clearAll() {
        memset(connections, 0, sizeof(connections));
    }

    /**
     * @brief Get pointer to internal connection array (for debugging/status)
     * @return Pointer to connections array
     */
    const Connection* getConnections() const {
        return connections;
    }

private:
    Connection connections[MAX_WS_CLIENTS];  // ~128 bytes
};

#endif // CONNECTION_MANAGER_H
