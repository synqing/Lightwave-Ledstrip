#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

/**
 * @file RateLimiter.h
 * @brief Lightweight rate limiting for LightwaveOS API protection
 *
 * Implements sliding window rate limiting for both HTTP and WebSocket requests.
 * Uses a fixed-size array of IP entries with LRU eviction when full.
 *
 * Features:
 * - Separate limits for HTTP (20/sec) and WebSocket (50/sec) traffic
 * - Automatic blocking for 5 seconds when limit exceeded
 * - LRU eviction when tracking table is full
 *
 * RAM Cost: ~400 bytes (8 IP entries * ~48 bytes each)
 */

#include <Arduino.h>
#include <WiFi.h>

class RateLimiter {
public:
    // Configuration constants
    static constexpr uint8_t MAX_TRACKED_IPS = 8;       // Number of IPs to track
    static constexpr uint32_t WINDOW_SIZE_MS = 1000;    // 1 second sliding window
    static constexpr uint16_t HTTP_LIMIT = 20;           // Max HTTP requests per window
    static constexpr uint16_t WS_LIMIT = 50;             // Max WebSocket messages per window
    static constexpr uint32_t BLOCK_DURATION_MS = 5000;  // Block duration when limit exceeded

    /**
     * @brief Per-IP rate limiting entry
     */
    struct Entry {
        IPAddress ip;           // Client IP address
        uint32_t windowStart;   // Start of current window (millis)
        uint16_t httpCount;     // HTTP requests in current window
        uint16_t wsCount;       // WebSocket messages in current window
        uint32_t blockedUntil;  // Time when block expires (0 = not blocked)
    };

    RateLimiter() {
        memset(entries, 0, sizeof(entries));
    }

    /**
     * @brief Check and record an HTTP request
     * @param ip Client IP address
     * @return true if request is allowed, false if rate limited
     */
    bool checkHTTP(IPAddress ip) {
        Entry* entry = findOrCreate(ip);
        if (!entry) return true; // Can't track, allow the request

        uint32_t now = millis();

        // Check if currently blocked
        if (entry->blockedUntil > now) {
            return false;
        }

        // Reset window if expired
        if (now - entry->windowStart > WINDOW_SIZE_MS) {
            entry->windowStart = now;
            entry->httpCount = 0;
            entry->wsCount = 0;
        }

        // Check limit
        if (entry->httpCount >= HTTP_LIMIT) {
            entry->blockedUntil = now + BLOCK_DURATION_MS;
            return false;
        }

        entry->httpCount++;
        return true;
    }

    /**
     * @brief Check and record a WebSocket message
     * @param ip Client IP address
     * @return true if message is allowed, false if rate limited
     */
    bool checkWebSocket(IPAddress ip) {
        Entry* entry = findOrCreate(ip);
        if (!entry) return true; // Can't track, allow the message

        uint32_t now = millis();

        // Check if currently blocked
        if (entry->blockedUntil > now) {
            return false;
        }

        // Reset window if expired
        if (now - entry->windowStart > WINDOW_SIZE_MS) {
            entry->windowStart = now;
            entry->httpCount = 0;
            entry->wsCount = 0;
        }

        // Check limit
        if (entry->wsCount >= WS_LIMIT) {
            entry->blockedUntil = now + BLOCK_DURATION_MS;
            return false;
        }

        entry->wsCount++;
        return true;
    }

    /**
     * @brief Check if an IP is currently blocked
     * @param ip Client IP address
     * @return true if blocked, false if allowed
     */
    bool isBlocked(IPAddress ip) {
        for (uint8_t i = 0; i < MAX_TRACKED_IPS; i++) {
            if (entries[i].ip == ip) {
                return entries[i].blockedUntil > millis();
            }
        }
        return false;
    }

    /**
     * @brief Get remaining time until block expires
     * @param ip Client IP address
     * @return Remaining block time in milliseconds, 0 if not blocked
     */
    uint32_t getBlockTimeRemaining(IPAddress ip) {
        uint32_t now = millis();
        for (uint8_t i = 0; i < MAX_TRACKED_IPS; i++) {
            if (entries[i].ip == ip && entries[i].blockedUntil > now) {
                return entries[i].blockedUntil - now;
            }
        }
        return 0;
    }

    /**
     * @brief Clear rate limiting state for an IP
     * @param ip Client IP address to clear
     */
    void clearIP(IPAddress ip) {
        for (uint8_t i = 0; i < MAX_TRACKED_IPS; i++) {
            if (entries[i].ip == ip) {
                entries[i].ip = IPAddress(0, 0, 0, 0);
                entries[i].httpCount = 0;
                entries[i].wsCount = 0;
                entries[i].blockedUntil = 0;
                return;
            }
        }
    }

    /**
     * @brief Clear all rate limiting state
     */
    void clearAll() {
        memset(entries, 0, sizeof(entries));
    }

    /**
     * @brief Get current statistics for an IP
     * @param ip Client IP address
     * @param httpCount Output: HTTP request count in current window
     * @param wsCount Output: WebSocket message count in current window
     * @return true if IP is being tracked, false otherwise
     */
    bool getStats(IPAddress ip, uint16_t& httpCount, uint16_t& wsCount) {
        for (uint8_t i = 0; i < MAX_TRACKED_IPS; i++) {
            if (entries[i].ip == ip) {
                // Check if window is still valid
                if (millis() - entries[i].windowStart <= WINDOW_SIZE_MS) {
                    httpCount = entries[i].httpCount;
                    wsCount = entries[i].wsCount;
                } else {
                    httpCount = 0;
                    wsCount = 0;
                }
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get number of currently tracked IPs
     * @return Number of active entries
     */
    uint8_t getTrackedCount() {
        uint8_t count = 0;
        for (uint8_t i = 0; i < MAX_TRACKED_IPS; i++) {
            if (entries[i].ip != IPAddress(0, 0, 0, 0)) {
                count++;
            }
        }
        return count;
    }

private:
    Entry entries[MAX_TRACKED_IPS];  // ~384 bytes

    /**
     * @brief Find existing entry or create new one for IP
     * Uses LRU eviction when table is full
     */
    Entry* findOrCreate(IPAddress ip) {
        // Find existing entry
        for (uint8_t i = 0; i < MAX_TRACKED_IPS; i++) {
            if (entries[i].ip == ip) {
                return &entries[i];
            }
        }

        // Find empty slot
        for (uint8_t i = 0; i < MAX_TRACKED_IPS; i++) {
            if (entries[i].ip == IPAddress(0, 0, 0, 0)) {
                entries[i].ip = ip;
                entries[i].windowStart = millis();
                entries[i].httpCount = 0;
                entries[i].wsCount = 0;
                entries[i].blockedUntil = 0;
                return &entries[i];
            }
        }

        // Table full - evict oldest (LRU) entry
        uint32_t oldest = 0xFFFFFFFF;
        uint8_t oldestIdx = 0;
        for (uint8_t i = 0; i < MAX_TRACKED_IPS; i++) {
            if (entries[i].windowStart < oldest) {
                oldest = entries[i].windowStart;
                oldestIdx = i;
            }
        }

        // Reset the oldest entry for new IP
        entries[oldestIdx].ip = ip;
        entries[oldestIdx].windowStart = millis();
        entries[oldestIdx].httpCount = 0;
        entries[oldestIdx].wsCount = 0;
        entries[oldestIdx].blockedUntil = 0;
        return &entries[oldestIdx];
    }
};

#endif // RATE_LIMITER_H
