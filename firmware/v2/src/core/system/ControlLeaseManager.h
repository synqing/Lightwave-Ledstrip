/**
 * @file ControlLeaseManager.h
 * @brief Global control lease manager for exclusive dashboard control sessions
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_CONTROL_LEASE

#include <Arduino.h>
#include <stdint.h>
#include <functional>
#if defined(ESP32) && !defined(NATIVE_BUILD)
#include <freertos/portmacro.h>
#endif

namespace lightwaveos {
namespace core {
namespace system {

class ControlLeaseManager {
public:
    static constexpr size_t LEASE_TOKEN_BYTES = 16; // 128-bit entropy minimum
    static constexpr uint32_t DEFAULT_TTL_MS = 5000;
    static constexpr uint32_t DEFAULT_HEARTBEAT_INTERVAL_MS = 1000;

    enum class LeaseEvent : uint8_t {
        None = 0,
        Acquired,
        Heartbeat,
        Released,
        Expired,
        RejectedLocked,
        BlockedWs,
        BlockedRest,
        BlockedLocalEncoder,
        BlockedLocalSerial
    };

    struct LeaseState {
        bool active = false;
        uint32_t ownerWsClientId = 0;
        String leaseId;
        String leaseToken;
        String scope = "global";
        String ownerClientName;
        String ownerInstanceId;
        uint32_t acquiredAtMs = 0;
        uint32_t expiresAtMs = 0;
        uint32_t ttlMs = DEFAULT_TTL_MS;
        uint32_t heartbeatIntervalMs = DEFAULT_HEARTBEAT_INTERVAL_MS;
        bool takeoverAllowed = false;
    };

    struct StatusCounters {
        uint32_t blockedWsCommands = 0;
        uint32_t blockedRestRequests = 0;
        uint32_t blockedLocalEncoderInputs = 0;
        uint32_t blockedLocalSerialInputs = 0;
        uint32_t lastLeaseEventMs = 0;
    };

    struct AcquireResult {
        bool success = false;
        bool locked = false;
        bool reacquired = false;
        LeaseState state;
        uint32_t remainingMs = 0;
    };

    struct HeartbeatResult {
        bool success = false;
        bool expired = false;
        bool invalid = false;
        bool locked = false;
        LeaseState state;
        uint32_t remainingMs = 0;
    };

    struct ReleaseResult {
        bool success = false;
        bool invalid = false;
        bool released = false;
        LeaseState state;
    };

    enum class MutationSource : uint8_t {
        Ws = 0,
        Rest,
        LocalEncoder,
        LocalSerial
    };

    enum class MutationError : uint8_t {
        None = 0,
        LeaseRequired,
        LeaseInvalid,
        ControlLocked,
        LeaseExpired
    };

    struct MutationCheckResult {
        bool allowed = true;
        bool leaseActive = false;
        MutationError error = MutationError::None;
        LeaseState state;
        uint32_t remainingMs = 0;
    };

    using StateChangeCallback = std::function<void(LeaseEvent, const LeaseState&)>;

    static void setStateChangeCallback(StateChangeCallback callback);

    static AcquireResult acquire(uint32_t wsClientId,
                                 const char* clientName,
                                 const char* clientInstanceId,
                                 const char* scope = "global");

    static HeartbeatResult heartbeat(uint32_t wsClientId,
                                     const char* leaseId,
                                     const char* leaseToken);

    static ReleaseResult release(uint32_t wsClientId,
                                 const char* leaseId,
                                 const char* leaseToken,
                                 const char* reason = "user_release");

    static ReleaseResult releaseByDisconnect(uint32_t wsClientId,
                                             const char* reason = "ws_disconnect");

    static LeaseState getState();
    static StatusCounters getCounters();

    static bool hasActiveLease();
    static bool isWsOwner(uint32_t wsClientId);

    static bool validateRestLeaseToken(const String& leaseToken, const String& leaseId, bool& idMismatch);

    static MutationCheckResult checkMutationPermission(MutationSource source,
                                                       uint32_t wsClientId = 0,
                                                       const char* leaseToken = nullptr,
                                                       const char* leaseId = nullptr);

    static uint32_t getRemainingMs();

    static void noteBlockedWsCommand(const char* command);
    static void noteBlockedRestCommand(const char* command);
    static void noteBlockedLocalEncoder(const char* command);
    static void noteBlockedLocalSerial(const char* command);

    static void maybeExpireLease();

private:
#if defined(ESP32) && !defined(NATIVE_BUILD)
    using LockType = portMUX_TYPE;
#else
    struct LockType {
        volatile uint8_t guard = 0;
    };
#endif

    static LockType s_mux;
    static LeaseState s_state;
    static StatusCounters s_counters;
    static StateChangeCallback s_stateChangeCallback;

    static void lock();
    static void unlock();

    static bool constantTimeEquals(const char* a, const char* b);
    static String generateLeaseId();
    static String generateLeaseToken();
    static void emitTelemetry(LeaseEvent event,
                              const LeaseState& state,
                              const char* source,
                              const char* command,
                              const char* reason,
                              uint32_t remainingMs);
    static void publishStateChange(LeaseEvent event, const LeaseState& state);
    static uint32_t remainingMsFromState(const LeaseState& state, uint32_t nowMs);
};

} // namespace system
} // namespace core
} // namespace lightwaveos

#endif // FEATURE_CONTROL_LEASE
