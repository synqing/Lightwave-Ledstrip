// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file OtaSessionLock.h
 * @brief Thread-safe OTA session state guard for cross-transport exclusion
 *
 * Provides a single, shared lock that ensures only ONE OTA session can be
 * active at any time, regardless of transport (REST multipart upload or
 * WebSocket chunked upload).
 *
 * Threading model:
 *   - AsyncWebServer HTTP handlers   -> async_tcp task (Core 0)
 *   - AsyncWebServer WebSocket handlers -> async_tcp task (Core 0)
 *   - WiFiManager state machine      -> its own FreeRTOS task (Core 0)
 *   - Main render loop               -> Core 1
 *
 * Because readers (WiFiManager) and writers (async_tcp) run on DIFFERENT
 * FreeRTOS tasks, bare reads/writes to the static flags are data races.
 * We use a portMUX_TYPE spinlock for all accesses because:
 *   1. Critical sections are microsecond-level (flag read/write only)
 *   2. Spinlocks work from any context (task, ISR - though ISR is N/A here)
 *   3. No heap allocation required (unlike SemaphoreHandle_t)
 *
 * Usage:
 *   // Before starting an OTA session:
 *   if (!OtaSessionLock::tryAcquire(OtaTransport::WebSocket)) {
 *       // Another OTA is in progress — reject
 *   }
 *
 *   // When OTA completes or fails:
 *   OtaSessionLock::release();
 *
 *   // From WiFiManager or any observer:
 *   if (OtaSessionLock::isOtaInProgress()) {
 *       // Suppress STA retry
 *   }
 */

#pragma once

#include <Arduino.h>
#include <freertos/portmacro.h>

namespace lightwaveos {
namespace core {
namespace system {

/**
 * @brief Identifies which transport owns the current OTA session.
 */
enum class OtaTransport : uint8_t {
    None = 0,
    Rest,
    WebSocket
};

/**
 * @brief Global OTA session lock — at most one OTA active across all transports.
 *
 * All methods are static. The spinlock is initialized at file scope (no
 * dynamic init required).
 */
class OtaSessionLock {
public:
    /**
     * @brief Attempt to acquire the OTA session lock.
     *
     * If no OTA session is active, marks the session as active for the
     * given transport and returns true. If another OTA session is already
     * active (same or different transport), returns false.
     *
     * @param transport Which transport is requesting the lock
     * @return true if the lock was acquired, false if another OTA is active
     */
    static bool tryAcquire(OtaTransport transport) {
        bool acquired = false;
        taskENTER_CRITICAL(&s_mux);
        if (s_transport == OtaTransport::None) {
            s_transport = transport;
            acquired = true;
        }
        taskEXIT_CRITICAL(&s_mux);
        return acquired;
    }

    /**
     * @brief Release the OTA session lock.
     *
     * Safe to call even if no session is active (idempotent).
     */
    static void release() {
        taskENTER_CRITICAL(&s_mux);
        s_transport = OtaTransport::None;
        taskEXIT_CRITICAL(&s_mux);
    }

    /**
     * @brief Check if ANY OTA session is currently in progress.
     *
     * Thread-safe. Called by WiFiManager to suppress STA retry during OTA.
     *
     * @return true if an OTA session is active on any transport
     */
    static bool isOtaInProgress() {
        bool active;
        taskENTER_CRITICAL(&s_mux);
        active = (s_transport != OtaTransport::None);
        taskEXIT_CRITICAL(&s_mux);
        return active;
    }

    /**
     * @brief Get which transport currently holds the OTA lock.
     *
     * Thread-safe.
     *
     * @return The active transport, or OtaTransport::None
     */
    static OtaTransport activeTransport() {
        OtaTransport t;
        taskENTER_CRITICAL(&s_mux);
        t = s_transport;
        taskEXIT_CRITICAL(&s_mux);
        return t;
    }

private:
    static inline portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
    static inline OtaTransport s_transport = OtaTransport::None;
};

} // namespace system
} // namespace core
} // namespace lightwaveos
