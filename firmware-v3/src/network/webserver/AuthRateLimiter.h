// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AuthRateLimiter.h
 * @brief Per-IP authentication rate limiter for brute force protection
 *
 * Tracks failed authentication attempts per IP address and blocks IPs
 * that exceed the failure threshold within a time window.
 *
 * Features:
 * - 5 failed attempts per IP within 60 seconds triggers block
 * - Block duration: 5 minutes
 * - Successful auth resets failure counter
 * - LRU eviction when tracking table is full
 * - Provides remaining block time for Retry-After header
 *
 * RAM Cost: ~192 bytes (8 IP entries * ~24 bytes each)
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_WEB_SERVER && FEATURE_API_AUTH

#include <Arduino.h>
#include <WiFi.h>
#include "RateLimiter.h"  // For ITimeSource

namespace lightwaveos {
namespace network {
namespace webserver {

// ============================================================================
// Auth Rate Limiter Configuration
// ============================================================================

namespace AuthRateLimitConfig {
    constexpr uint8_t MAX_TRACKED_IPS = 8;           // Number of IPs to track
    constexpr uint8_t MAX_FAILED_ATTEMPTS = 5;       // Failures before block
    constexpr uint32_t WINDOW_SIZE_MS = 60000;       // 60 second window
    constexpr uint32_t BLOCK_DURATION_MS = 300000;   // 5 minute block
    constexpr uint16_t RETRY_AFTER_SECONDS = 300;    // Retry-After header value
}

// ============================================================================
// Auth Rate Limiter
// ============================================================================

/**
 * @brief Per-IP rate limiter for authentication failures
 *
 * Tracks failed authentication attempts per IP address. When the failure
 * threshold is exceeded within the time window, the IP is blocked.
 */
class AuthRateLimiter {
public:
    /**
     * @brief Per-IP auth tracking entry
     */
    struct Entry {
        IPAddress ip;           // Client IP address
        uint32_t windowStart;   // Start of current window (millis)
        uint8_t failureCount;   // Failed attempts in current window
        uint32_t blockedUntil;  // Time when block expires (0 = not blocked)
    };

    /**
     * @brief Construct AuthRateLimiter with time source
     * @param timeSource Time source (defaults to Arduino millis)
     */
    explicit AuthRateLimiter(ITimeSource* timeSource = nullptr)
        : m_timeSource(timeSource ? timeSource : &m_defaultTimeSource)
    {
        memset(m_entries, 0, sizeof(m_entries));
    }

    /**
     * @brief Check if an IP is currently blocked from authentication
     * @param ip Client IP address
     * @return true if blocked, false if auth attempts are allowed
     */
    bool isBlocked(IPAddress ip) const {
        uint32_t now = m_timeSource->millis();
        for (uint8_t i = 0; i < AuthRateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                return m_entries[i].blockedUntil > now;
            }
        }
        return false;
    }

    /**
     * @brief Record a failed authentication attempt
     *
     * Increments failure counter for the IP. If threshold exceeded,
     * the IP is blocked.
     *
     * @param ip Client IP address
     * @return true if IP is now blocked (threshold exceeded), false if still allowed
     */
    bool recordFailure(IPAddress ip) {
        Entry* entry = findOrCreate(ip);
        if (!entry) return false;  // Can't track

        uint32_t now = m_timeSource->millis();

        // Check if currently blocked
        if (entry->blockedUntil > now) {
            return true;  // Already blocked
        }

        // Reset window if expired
        if (now - entry->windowStart > AuthRateLimitConfig::WINDOW_SIZE_MS) {
            entry->windowStart = now;
            entry->failureCount = 0;
        }

        // Increment failure count
        entry->failureCount++;

        // Check if threshold exceeded
        if (entry->failureCount >= AuthRateLimitConfig::MAX_FAILED_ATTEMPTS) {
            entry->blockedUntil = now + AuthRateLimitConfig::BLOCK_DURATION_MS;
            return true;  // Now blocked
        }

        return false;  // Still allowed
    }

    /**
     * @brief Record a successful authentication (resets failure counter)
     * @param ip Client IP address
     */
    void recordSuccess(IPAddress ip) {
        for (uint8_t i = 0; i < AuthRateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                // Reset failure count but keep entry for potential future failures
                m_entries[i].failureCount = 0;
                m_entries[i].windowStart = m_timeSource->millis();
                // Note: Don't clear blockedUntil - if blocked, stay blocked until expiry
                return;
            }
        }
    }

    /**
     * @brief Get remaining time until block expires
     * @param ip Client IP address
     * @return Remaining block time in seconds (for Retry-After header), 0 if not blocked
     */
    uint32_t getRetryAfterSeconds(IPAddress ip) const {
        uint32_t now = m_timeSource->millis();
        for (uint8_t i = 0; i < AuthRateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip && m_entries[i].blockedUntil > now) {
                return (m_entries[i].blockedUntil - now + 999) / 1000;  // Round up to seconds
            }
        }
        return AuthRateLimitConfig::RETRY_AFTER_SECONDS;  // Default retry time
    }

    /**
     * @brief Get current failure count for an IP
     * @param ip Client IP address
     * @return Current failure count in window, or 0 if not tracked
     */
    uint8_t getFailureCount(IPAddress ip) const {
        uint32_t now = m_timeSource->millis();
        for (uint8_t i = 0; i < AuthRateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                if (now - m_entries[i].windowStart <= AuthRateLimitConfig::WINDOW_SIZE_MS) {
                    return m_entries[i].failureCount;
                }
                return 0;  // Window expired
            }
        }
        return 0;
    }

private:
    ITimeSource* m_timeSource;
    ArduinoTimeSource m_defaultTimeSource;
    mutable Entry m_entries[AuthRateLimitConfig::MAX_TRACKED_IPS];  // ~192 bytes

    /**
     * @brief Find existing entry or create new one for IP
     * Uses LRU eviction when table is full
     */
    Entry* findOrCreate(IPAddress ip) {
        // Find existing entry
        for (uint8_t i = 0; i < AuthRateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                return &m_entries[i];
            }
        }

        // Find empty slot
        for (uint8_t i = 0; i < AuthRateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == IPAddress(0, 0, 0, 0)) {
                m_entries[i].ip = ip;
                m_entries[i].windowStart = m_timeSource->millis();
                m_entries[i].failureCount = 0;
                m_entries[i].blockedUntil = 0;
                return &m_entries[i];
            }
        }

        // Table full - evict oldest (LRU) entry
        uint32_t oldest = 0xFFFFFFFF;
        uint8_t oldestIdx = 0;
        for (uint8_t i = 0; i < AuthRateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].windowStart < oldest) {
                oldest = m_entries[i].windowStart;
                oldestIdx = i;
            }
        }

        // Reset the oldest entry for new IP
        m_entries[oldestIdx].ip = ip;
        m_entries[oldestIdx].windowStart = m_timeSource->millis();
        m_entries[oldestIdx].failureCount = 0;
        m_entries[oldestIdx].blockedUntil = 0;
        return &m_entries[oldestIdx];
    }
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_API_AUTH
