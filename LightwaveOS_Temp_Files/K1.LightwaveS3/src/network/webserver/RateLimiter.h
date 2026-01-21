/**
 * @file RateLimiter.h
 * @brief Per-IP token bucket rate limiter with sliding window
 *
 * Extracted from WebServer.h for better separation of concerns and testability.
 * Uses an injected time source to enable host-side unit testing.
 *
 * Features:
 * - Separate limits for HTTP (20/sec) and WebSocket (50/sec)
 * - Automatic blocking for 5 seconds when limit exceeded
 * - LRU eviction when tracking table is full
 * - Provides remaining block time for Retry-After header
 *
 * RAM Cost: ~400 bytes (8 IP entries * ~48 bytes each)
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace lightwaveos {
namespace network {
namespace webserver {

// ============================================================================
// Time Source Interface (for testing)
// ============================================================================

/**
 * @brief Abstract time source interface for dependency injection
 */
class ITimeSource {
public:
    virtual ~ITimeSource() = default;
    virtual uint32_t millis() const = 0;
};

/**
 * @brief Default time source using Arduino millis()
 */
class ArduinoTimeSource : public ITimeSource {
public:
    uint32_t millis() const override {
        return ::millis();
    }
};

// ============================================================================
// Rate Limiter Configuration
// ============================================================================

namespace RateLimitConfig {
    constexpr uint8_t MAX_TRACKED_IPS = 8;         // Number of IPs to track
    constexpr uint16_t HTTP_LIMIT = 20;             // Max HTTP requests per window
    constexpr uint16_t WS_LIMIT = 50;               // Max WebSocket messages per window
    constexpr uint32_t WINDOW_SIZE_MS = 1000;       // 1 second sliding window
    constexpr uint32_t BLOCK_DURATION_MS = 5000;    // Block duration when limit exceeded
    constexpr uint8_t RETRY_AFTER_SECONDS = 5;      // Retry-After header value
}

// ============================================================================
// Rate Limiter
// ============================================================================

/**
 * @brief Per-IP token bucket rate limiter with sliding window
 *
 * Tracks rate limits per IP address for HTTP and per client for WebSocket.
 * Uses LRU eviction when tracking table is full.
 */
class RateLimiter {
public:
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

    /**
     * @brief Construct RateLimiter with time source
     * @param timeSource Time source (defaults to Arduino millis)
     */
    explicit RateLimiter(ITimeSource* timeSource = nullptr)
        : m_timeSource(timeSource ? timeSource : &m_defaultTimeSource)
    {
        memset(m_entries, 0, sizeof(m_entries));
    }

    /**
     * @brief Check and record an HTTP request
     * @param ip Client IP address
     * @return true if request is allowed, false if rate limited
     */
    bool checkHTTP(IPAddress ip) {
        Entry* entry = findOrCreate(ip);
        if (!entry) return true; // Can't track, allow the request

        uint32_t now = m_timeSource->millis();

        // Check if currently blocked
        if (entry->blockedUntil > now) {
            return false;
        }

        // Reset window if expired
        if (now - entry->windowStart > RateLimitConfig::WINDOW_SIZE_MS) {
            entry->windowStart = now;
            entry->httpCount = 0;
            entry->wsCount = 0;
        }

        // Check limit
        if (entry->httpCount >= RateLimitConfig::HTTP_LIMIT) {
            entry->blockedUntil = now + RateLimitConfig::BLOCK_DURATION_MS;
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

        uint32_t now = m_timeSource->millis();

        // Check if currently blocked
        if (entry->blockedUntil > now) {
            return false;
        }

        // Reset window if expired
        if (now - entry->windowStart > RateLimitConfig::WINDOW_SIZE_MS) {
            entry->windowStart = now;
            entry->httpCount = 0;
            entry->wsCount = 0;
        }

        // Check limit
        if (entry->wsCount >= RateLimitConfig::WS_LIMIT) {
            entry->blockedUntil = now + RateLimitConfig::BLOCK_DURATION_MS;
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
    bool isBlocked(IPAddress ip) const {
        uint32_t now = m_timeSource->millis();
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                return m_entries[i].blockedUntil > now;
            }
        }
        return false;
    }

    /**
     * @brief Get remaining time until block expires
     * @param ip Client IP address
     * @return Remaining block time in seconds (for Retry-After header), 0 if not blocked
     */
    uint32_t getRetryAfterSeconds(IPAddress ip) const {
        uint32_t now = m_timeSource->millis();
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip && m_entries[i].blockedUntil > now) {
                return (m_entries[i].blockedUntil - now + 999) / 1000; // Round up to seconds
            }
        }
        return RateLimitConfig::RETRY_AFTER_SECONDS; // Default retry time
    }

    /**
     * @brief Get current HTTP request count for an IP
     * @param ip Client IP address
     * @return Current request count in window, or 0 if not tracked
     */
    uint16_t getHttpCount(IPAddress ip) const {
        uint32_t now = m_timeSource->millis();
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                if (now - m_entries[i].windowStart <= RateLimitConfig::WINDOW_SIZE_MS) {
                    return m_entries[i].httpCount;
                }
                return 0;
            }
        }
        return 0;
    }

    /**
     * @brief Get current WebSocket message count for an IP
     * @param ip Client IP address
     * @return Current message count in window, or 0 if not tracked
     */
    uint16_t getWsCount(IPAddress ip) const {
        uint32_t now = m_timeSource->millis();
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                if (now - m_entries[i].windowStart <= RateLimitConfig::WINDOW_SIZE_MS) {
                    return m_entries[i].wsCount;
                }
                return 0;
            }
        }
        return 0;
    }

private:
    ITimeSource* m_timeSource;
    ArduinoTimeSource m_defaultTimeSource;
    mutable Entry m_entries[RateLimitConfig::MAX_TRACKED_IPS];  // ~384 bytes

    /**
     * @brief Find existing entry or create new one for IP
     * Uses LRU eviction when table is full
     */
    Entry* findOrCreate(IPAddress ip) {
        // Find existing entry
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                return &m_entries[i];
            }
        }

        // Find empty slot
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == IPAddress(0, 0, 0, 0)) {
                m_entries[i].ip = ip;
                m_entries[i].windowStart = m_timeSource->millis();
                m_entries[i].httpCount = 0;
                m_entries[i].wsCount = 0;
                m_entries[i].blockedUntil = 0;
                return &m_entries[i];
            }
        }

        // Table full - evict oldest (LRU) entry
        uint32_t oldest = 0xFFFFFFFF;
        uint8_t oldestIdx = 0;
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].windowStart < oldest) {
                oldest = m_entries[i].windowStart;
                oldestIdx = i;
            }
        }

        // Reset the oldest entry for new IP
        m_entries[oldestIdx].ip = ip;
        m_entries[oldestIdx].windowStart = m_timeSource->millis();
        m_entries[oldestIdx].httpCount = 0;
        m_entries[oldestIdx].wsCount = 0;
        m_entries[oldestIdx].blockedUntil = 0;
        return &m_entries[oldestIdx];
    }
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

