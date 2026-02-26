/**
 * @file ControlLeaseManager.cpp
 * @brief Global control lease manager implementation
 */

#include "ControlLeaseManager.h"

#if FEATURE_CONTROL_LEASE

#include "../../utils/Log.h"
#include <cstring>

#if defined(ESP32) && !defined(NATIVE_BUILD)
#include <freertos/portmacro.h>
#include <esp_system.h>
#endif

#undef LW_LOG_TAG
#define LW_LOG_TAG "ControlLease"

namespace {
bool isExpiredWrapSafe(uint32_t nowMs, uint32_t expiresAtMs) {
    return static_cast<int32_t>(nowMs - expiresAtMs) >= 0;
}

uint32_t remainingWrapSafe(uint32_t nowMs, uint32_t expiresAtMs) {
    const int32_t delta = static_cast<int32_t>(expiresAtMs - nowMs);
    return (delta <= 0) ? 0u : static_cast<uint32_t>(delta);
}

bool isEmptyText(const char* value) {
    return (value == nullptr) || (value[0] == '\0');
}
} // namespace

namespace lightwaveos {
namespace core {
namespace system {

ControlLeaseManager::LockType ControlLeaseManager::s_mux =
#if defined(ESP32) && !defined(NATIVE_BUILD)
    portMUX_INITIALIZER_UNLOCKED;
#else
    {};
#endif

ControlLeaseManager::LeaseState ControlLeaseManager::s_state{};
ControlLeaseManager::StatusCounters ControlLeaseManager::s_counters{};
ControlLeaseManager::StateChangeCallback ControlLeaseManager::s_stateChangeCallback = nullptr;

void ControlLeaseManager::lock() {
#if defined(ESP32) && !defined(NATIVE_BUILD)
    taskENTER_CRITICAL(&s_mux);
#else
    while (__sync_lock_test_and_set(&s_mux.guard, 1) != 0) {
    }
#endif
}

void ControlLeaseManager::unlock() {
#if defined(ESP32) && !defined(NATIVE_BUILD)
    taskEXIT_CRITICAL(&s_mux);
#else
    __sync_lock_release(&s_mux.guard);
#endif
}

bool ControlLeaseManager::constantTimeEquals(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
    const size_t aLen = strlen(a);
    const size_t bLen = strlen(b);
    size_t maxLen = aLen > bLen ? aLen : bLen;
    volatile uint8_t diff = static_cast<uint8_t>(aLen ^ bLen);
    for (size_t i = 0; i < maxLen; ++i) {
        const uint8_t av = i < aLen ? static_cast<uint8_t>(a[i]) : 0;
        const uint8_t bv = i < bLen ? static_cast<uint8_t>(b[i]) : 0;
        diff |= static_cast<uint8_t>(av ^ bv);
    }
    return diff == 0;
}

String ControlLeaseManager::generateLeaseId() {
#if defined(ESP32) && !defined(NATIVE_BUILD)
    uint32_t a = esp_random();
    uint32_t b = esp_random();
#else
    uint32_t a = static_cast<uint32_t>(millis()) ^ 0xA5A5A5A5u;
    uint32_t b = static_cast<uint32_t>(micros()) ^ 0x5A5A5A5Au;
#endif

    char out[32];
    snprintf(out, sizeof(out), "cl_%08lx%04lx",
             static_cast<unsigned long>(a),
             static_cast<unsigned long>(b & 0xFFFFu));
    return String(out);
}

String ControlLeaseManager::generateLeaseToken() {
    static const char* BASE64URL = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    uint8_t raw[LEASE_TOKEN_BYTES];
#if defined(ESP32) && !defined(NATIVE_BUILD)
    esp_fill_random(raw, sizeof(raw));
#else
    uint32_t seed = static_cast<uint32_t>(millis()) ^ static_cast<uint32_t>(micros());
    for (size_t i = 0; i < sizeof(raw); ++i) {
        seed ^= (seed << 13);
        seed ^= (seed >> 17);
        seed ^= (seed << 5);
        raw[i] = static_cast<uint8_t>(seed & 0xFF);
    }
#endif

    char token[((LEASE_TOKEN_BYTES + 2) / 3) * 4 + 1];
    size_t outIdx = 0;
    for (size_t i = 0; i < sizeof(raw); i += 3) {
        uint32_t triple = static_cast<uint32_t>(raw[i]) << 16;
        if (i + 1 < sizeof(raw)) triple |= static_cast<uint32_t>(raw[i + 1]) << 8;
        if (i + 2 < sizeof(raw)) triple |= static_cast<uint32_t>(raw[i + 2]);

        token[outIdx++] = BASE64URL[(triple >> 18) & 0x3F];
        token[outIdx++] = BASE64URL[(triple >> 12) & 0x3F];
        if (i + 1 < sizeof(raw)) token[outIdx++] = BASE64URL[(triple >> 6) & 0x3F];
        if (i + 2 < sizeof(raw)) token[outIdx++] = BASE64URL[triple & 0x3F];
    }
    token[outIdx] = '\0';
    return String(token);
}

void ControlLeaseManager::emitTelemetry(LeaseEvent event,
                                        const LeaseState& state,
                                        const char* source,
                                        const char* command,
                                        const char* reason,
                                        uint32_t remainingMs) {
    const char* eventName = "control.lease.unknown";
    switch (event) {
        case LeaseEvent::Acquired: eventName = "control.lease.acquired"; break;
        case LeaseEvent::Heartbeat: eventName = "control.lease.heartbeat"; break;
        case LeaseEvent::Released: eventName = "control.lease.released"; break;
        case LeaseEvent::Expired: eventName = "control.lease.expired"; break;
        case LeaseEvent::RejectedLocked: eventName = "control.lease.rejected_locked"; break;
        case LeaseEvent::BlockedWs: eventName = "control.command.blocked.ws"; break;
        case LeaseEvent::BlockedRest: eventName = "control.command.blocked.rest"; break;
        case LeaseEvent::BlockedLocalEncoder: eventName = "control.command.blocked.local.encoder"; break;
        case LeaseEvent::BlockedLocalSerial: eventName = "control.command.blocked.local.serial"; break;
        default: break;
    }

    char buf[640];
    int n = snprintf(
        buf,
        sizeof(buf),
        "{\"event\":\"%s\",\"ts_mono_ms\":%lu,\"schemaVersion\":\"1.0.0\","
        "\"leaseId\":\"%s\",\"source\":\"%s\",\"ownerWsClientId\":%lu,"
        "\"ownerClientName\":\"%s\",\"ownerInstanceId\":\"%s\","
        "\"remainingMs\":%lu,\"command\":\"%s\",\"reason\":\"%s\"}",
        eventName,
        static_cast<unsigned long>(millis()),
        state.leaseId.c_str(),
        source ? source : "",
        static_cast<unsigned long>(state.ownerWsClientId),
        state.ownerClientName.c_str(),
        state.ownerInstanceId.c_str(),
        static_cast<unsigned long>(remainingMs),
        command ? command : "",
        reason ? reason : ""
    );

    if (n > 0 && n < static_cast<int>(sizeof(buf))) {
        Serial.println(buf);
    }
}

void ControlLeaseManager::publishStateChange(LeaseEvent event, const LeaseState& state) {
    if (s_stateChangeCallback) {
        s_stateChangeCallback(event, state);
    }
}

uint32_t ControlLeaseManager::remainingMsFromState(const LeaseState& state, uint32_t nowMs) {
    if (!state.active) {
        return 0;
    }
    return remainingWrapSafe(nowMs, state.expiresAtMs);
}

void ControlLeaseManager::setStateChangeCallback(StateChangeCallback callback) {
    lock();
    s_stateChangeCallback = callback;
    unlock();
}

ControlLeaseManager::AcquireResult ControlLeaseManager::acquire(uint32_t wsClientId,
                                                                const char* clientName,
                                                                const char* clientInstanceId,
                                                                const char* scope) {
    maybeExpireLease();

    AcquireResult result;
    const uint32_t nowMs = millis();

    lock();
    if (s_state.active) {
        if (s_state.ownerWsClientId == wsClientId) {
            s_state.expiresAtMs = nowMs + s_state.ttlMs;
            result.success = true;
            result.reacquired = true;
            result.state = s_state;
            result.remainingMs = s_state.ttlMs;
            s_counters.lastLeaseEventMs = nowMs;
            unlock();

            emitTelemetry(LeaseEvent::Acquired, result.state, "ws", "control.acquire", "reacquire", result.remainingMs);
            publishStateChange(LeaseEvent::Acquired, result.state);
            return result;
        }

        result.locked = true;
        result.state = s_state;
        result.remainingMs = remainingMsFromState(s_state, nowMs);
        unlock();

        emitTelemetry(LeaseEvent::RejectedLocked, result.state, "ws", "control.acquire", "already_locked", result.remainingMs);
        return result;
    }

    s_state.active = true;
    s_state.ownerWsClientId = wsClientId;
    s_state.leaseId = generateLeaseId();
    s_state.leaseToken = generateLeaseToken();
    s_state.scope = scope ? String(scope) : String("global");
    s_state.ownerClientName = clientName ? String(clientName) : String("Unknown");
    s_state.ownerInstanceId = clientInstanceId ? String(clientInstanceId) : String("");
    s_state.acquiredAtMs = nowMs;
    s_state.expiresAtMs = nowMs + s_state.ttlMs;
    s_state.takeoverAllowed = false;

    result.success = true;
    result.state = s_state;
    result.remainingMs = s_state.ttlMs;
    s_counters.lastLeaseEventMs = nowMs;

    unlock();

    emitTelemetry(LeaseEvent::Acquired, result.state, "ws", "control.acquire", "acquired", result.remainingMs);
    publishStateChange(LeaseEvent::Acquired, result.state);
    return result;
}

ControlLeaseManager::HeartbeatResult ControlLeaseManager::heartbeat(uint32_t wsClientId,
                                                                    const char* leaseId,
                                                                    const char* leaseToken) {
    maybeExpireLease();

    HeartbeatResult result;
    const uint32_t nowMs = millis();

    lock();
    if (!s_state.active) {
        result.expired = true;
        unlock();
        return result;
    }

    if (s_state.ownerWsClientId != wsClientId) {
        result.locked = true;
        result.state = s_state;
        result.remainingMs = remainingMsFromState(s_state, nowMs);
        unlock();
        return result;
    }

    const bool idOk = constantTimeEquals(leaseId ? leaseId : "", s_state.leaseId.c_str());
    const bool tokenOk = constantTimeEquals(leaseToken ? leaseToken : "", s_state.leaseToken.c_str());
    if (!idOk || !tokenOk) {
        result.invalid = true;
        result.state = s_state;
        result.remainingMs = remainingMsFromState(s_state, nowMs);
        unlock();
        return result;
    }

    s_state.expiresAtMs = nowMs + s_state.ttlMs;
    result.success = true;
    result.state = s_state;
    result.remainingMs = s_state.ttlMs;
    s_counters.lastLeaseEventMs = nowMs;
    unlock();

    emitTelemetry(LeaseEvent::Heartbeat, result.state, "ws", "control.heartbeat", "heartbeat", result.remainingMs);
    publishStateChange(LeaseEvent::Heartbeat, result.state);
    return result;
}

ControlLeaseManager::ReleaseResult ControlLeaseManager::release(uint32_t wsClientId,
                                                                const char* leaseId,
                                                                const char* leaseToken,
                                                                const char* reason) {
    maybeExpireLease();

    ReleaseResult result;
    const uint32_t nowMs = millis();

    lock();
    if (!s_state.active) {
        result.success = true;
        result.released = true;
        unlock();
        return result;
    }

    if (s_state.ownerWsClientId != wsClientId) {
        result.invalid = true;
        result.state = s_state;
        unlock();
        return result;
    }

    const bool idOk = constantTimeEquals(leaseId ? leaseId : "", s_state.leaseId.c_str());
    const bool tokenOk = constantTimeEquals(leaseToken ? leaseToken : "", s_state.leaseToken.c_str());
    if (!idOk || !tokenOk) {
        result.invalid = true;
        result.state = s_state;
        unlock();
        return result;
    }

    LeaseState oldState = s_state;
    s_state = LeaseState{};
    s_counters.lastLeaseEventMs = nowMs;

    result.success = true;
    result.released = true;
    result.state = oldState;

    unlock();

    emitTelemetry(LeaseEvent::Released, oldState, "ws", "control.release", reason ? reason : "release", 0);
    publishStateChange(LeaseEvent::Released, oldState);
    return result;
}

ControlLeaseManager::ReleaseResult ControlLeaseManager::releaseByDisconnect(uint32_t wsClientId,
                                                                            const char* reason) {
    ReleaseResult result;
    lock();
    if (!s_state.active || s_state.ownerWsClientId != wsClientId) {
        unlock();
        result.success = true;
        result.released = false;
        return result;
    }

    LeaseState oldState = s_state;
    s_state = LeaseState{};
    s_counters.lastLeaseEventMs = millis();
    unlock();

    emitTelemetry(LeaseEvent::Released, oldState, "ws", "control.disconnect", reason ? reason : "disconnect", 0);
    publishStateChange(LeaseEvent::Released, oldState);

    result.success = true;
    result.released = true;
    result.state = oldState;
    return result;
}

ControlLeaseManager::LeaseState ControlLeaseManager::getState() {
    maybeExpireLease();
    lock();
    LeaseState copy = s_state;
    unlock();
    return copy;
}

ControlLeaseManager::StatusCounters ControlLeaseManager::getCounters() {
    lock();
    StatusCounters copy = s_counters;
    unlock();
    return copy;
}

bool ControlLeaseManager::hasActiveLease() {
    maybeExpireLease();
    lock();
    bool active = s_state.active;
    unlock();
    return active;
}

bool ControlLeaseManager::isWsOwner(uint32_t wsClientId) {
    maybeExpireLease();
    lock();
    bool isOwner = s_state.active && s_state.ownerWsClientId == wsClientId;
    unlock();
    return isOwner;
}

bool ControlLeaseManager::validateRestLeaseToken(const String& leaseToken,
                                                 const String& leaseId,
                                                 bool& idMismatch) {
    maybeExpireLease();
    idMismatch = false;

    lock();
    if (!s_state.active) {
        unlock();
        return true;
    }

    bool tokenOk = constantTimeEquals(leaseToken.c_str(), s_state.leaseToken.c_str());
    bool idOk = leaseId.length() == 0 || constantTimeEquals(leaseId.c_str(), s_state.leaseId.c_str());
    idMismatch = (leaseId.length() > 0 && !idOk);
    unlock();

    return tokenOk && idOk;
}

ControlLeaseManager::MutationCheckResult ControlLeaseManager::checkMutationPermission(MutationSource source,
                                                                                      uint32_t wsClientId,
                                                                                      const char* leaseToken,
                                                                                      const char* leaseId) {
    maybeExpireLease();

    MutationCheckResult result;
    const uint32_t nowMs = millis();

    lock();
    result.leaseActive = s_state.active;
    result.state = s_state;
    result.remainingMs = remainingMsFromState(s_state, nowMs);

    if (!s_state.active) {
        unlock();
        return result;
    }

    switch (source) {
        case MutationSource::Ws:
            if (s_state.ownerWsClientId != wsClientId) {
                result.allowed = false;
                result.error = MutationError::ControlLocked;
            }
            break;

        case MutationSource::Rest: {
            if (isEmptyText(leaseToken)) {
                result.allowed = false;
                result.error = MutationError::LeaseRequired;
                break;
            }

            const bool tokenOk = constantTimeEquals(leaseToken, s_state.leaseToken.c_str());
            const bool hasLeaseId = !isEmptyText(leaseId);
            const bool leaseIdOk = !hasLeaseId || constantTimeEquals(leaseId, s_state.leaseId.c_str());

            if (tokenOk && leaseIdOk) {
                break;
            }

            result.allowed = false;
            result.error = (hasLeaseId && leaseIdOk)
                ? MutationError::LeaseInvalid
                : MutationError::ControlLocked;
            break;
        }

        case MutationSource::LocalEncoder:
        case MutationSource::LocalSerial:
            result.allowed = false;
            result.error = MutationError::ControlLocked;
            break;

        default:
            break;
    }

    unlock();
    return result;
}

uint32_t ControlLeaseManager::getRemainingMs() {
    maybeExpireLease();
    lock();
    uint32_t remaining = remainingMsFromState(s_state, millis());
    unlock();
    return remaining;
}

void ControlLeaseManager::noteBlockedWsCommand(const char* command) {
    maybeExpireLease();
    LeaseState stateCopy;
    uint32_t remaining = 0;

    lock();
    ++s_counters.blockedWsCommands;
    s_counters.lastLeaseEventMs = millis();
    stateCopy = s_state;
    remaining = remainingMsFromState(s_state, millis());
    unlock();

    emitTelemetry(LeaseEvent::BlockedWs, stateCopy, "ws", command ? command : "", "blocked", remaining);
}

void ControlLeaseManager::noteBlockedRestCommand(const char* command) {
    maybeExpireLease();
    LeaseState stateCopy;
    uint32_t remaining = 0;

    lock();
    ++s_counters.blockedRestRequests;
    s_counters.lastLeaseEventMs = millis();
    stateCopy = s_state;
    remaining = remainingMsFromState(s_state, millis());
    unlock();

    emitTelemetry(LeaseEvent::BlockedRest, stateCopy, "rest", command ? command : "", "blocked", remaining);
}

void ControlLeaseManager::noteBlockedLocalEncoder(const char* command) {
    maybeExpireLease();
    LeaseState stateCopy;
    uint32_t remaining = 0;

    lock();
    ++s_counters.blockedLocalEncoderInputs;
    s_counters.lastLeaseEventMs = millis();
    stateCopy = s_state;
    remaining = remainingMsFromState(s_state, millis());
    unlock();

    emitTelemetry(LeaseEvent::BlockedLocalEncoder, stateCopy, "encoder", command ? command : "", "blocked", remaining);
}

void ControlLeaseManager::noteBlockedLocalSerial(const char* command) {
    maybeExpireLease();
    LeaseState stateCopy;
    uint32_t remaining = 0;

    lock();
    ++s_counters.blockedLocalSerialInputs;
    s_counters.lastLeaseEventMs = millis();
    stateCopy = s_state;
    remaining = remainingMsFromState(s_state, millis());
    unlock();

    emitTelemetry(LeaseEvent::BlockedLocalSerial, stateCopy, "serial", command ? command : "", "blocked", remaining);
}

void ControlLeaseManager::maybeExpireLease() {
    lock();
    if (!s_state.active) {
        unlock();
        return;
    }

    const uint32_t nowMs = millis();
    if (!isExpiredWrapSafe(nowMs, s_state.expiresAtMs)) {
        unlock();
        return;
    }

    LeaseState oldState = s_state;
    s_state = LeaseState{};
    s_counters.lastLeaseEventMs = nowMs;
    unlock();

    emitTelemetry(LeaseEvent::Expired, oldState, "ws", "control.expire", "heartbeat_timeout", 0);
    publishStateChange(LeaseEvent::Expired, oldState);
}

} // namespace system
} // namespace core
} // namespace lightwaveos

#endif // FEATURE_CONTROL_LEASE
