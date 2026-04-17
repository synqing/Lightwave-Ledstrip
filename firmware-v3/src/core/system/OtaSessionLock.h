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
     * @brief Hard ceiling on an OTA session duration, in milliseconds.
     *
     * If a session is still held after this long, the watchdog sweep in
     * WebServer::update() considers it stale and force-releases the lock
     * (plus, for WebSocket transport, clears the WS-local session state
     * and calls Update.abort()). Guards against half-open TCP sockets
     * dropping without a clean ota.abort — without this, the session flag
     * plus OtaLock stay held forever and every future OTA is rejected
     * with BUSY.
     */
    static constexpr uint32_t OTA_SESSION_MAX_MS = 5U * 60U * 1000U;  // 5 minutes

    /**
     * @brief Attempt to acquire the OTA session lock.
     *
     * If no OTA session is active, marks the session as active for the
     * given transport and returns true. If another OTA session is already
     * active (same or different transport), returns false.
     *
     * Captures the acquisition timestamp (millis()) so that a stale-session
     * sweep can detect wedged locks — see isStale() / checkTimeout().
     *
     * @param transport Which transport is requesting the lock
     * @return true if the lock was acquired, false if another OTA is active
     */
    static bool tryAcquire(OtaTransport transport) {
        bool acquired = false;
        taskENTER_CRITICAL(&s_mux);
        if (s_transport == OtaTransport::None) {
            s_transport = transport;
            s_sessionStartMs = millis();
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
        s_sessionStartMs = 0;
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

    /**
     * @brief Timestamp (millis()) when the current session was acquired.
     *
     * Thread-safe. Returns 0 when no session is active.
     */
    static uint32_t sessionStartMs() {
        uint32_t t;
        taskENTER_CRITICAL(&s_mux);
        t = s_sessionStartMs;
        taskEXIT_CRITICAL(&s_mux);
        return t;
    }

    /**
     * @brief Check whether the active session has exceeded OTA_SESSION_MAX_MS.
     *
     * Returns false if no session is active OR if the session is still
     * within the time budget. Handles millis() wrap by comparing the
     * unsigned difference.
     *
     * This method does NOT release the lock — callers (WebServer::update())
     * are responsible for performing the cleanup sequence (telemetry,
     * Update.abort(), WS-local state clear, release()). Keeping release
     * outside this check lets the caller match the abort path to the
     * transport that owns the session.
     *
     * @param nowMs Current millis() timestamp
     * @return true if session is active AND older than OTA_SESSION_MAX_MS
     */
    static bool isStale(uint32_t nowMs) {
        bool stale = false;
        taskENTER_CRITICAL(&s_mux);
        if (s_transport != OtaTransport::None && s_sessionStartMs != 0) {
            const uint32_t elapsed = nowMs - s_sessionStartMs;  // unsigned wrap-safe
            stale = (elapsed > OTA_SESSION_MAX_MS);
        }
        taskEXIT_CRITICAL(&s_mux);
        return stale;
    }

private:
    static inline portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
    static inline OtaTransport s_transport = OtaTransport::None;
    static inline uint32_t s_sessionStartMs = 0;  // millis() at tryAcquire; 0 when idle
};

} // namespace system
} // namespace core
} // namespace lightwaveos
