/**
 * Sync module implementations for native testing
 *
 * This file provides implementations of the sync components
 * that can run on the native host without ESP32 hardware.
 */

#ifndef NATIVE_BUILD
#define NATIVE_BUILD
#endif

#include "../../src/sync/DeviceUUID.h"
#include "../../src/sync/LeaderElection.h"
#include "../../src/sync/CommandSerializer.h"
#include "../../src/sync/StateSerializer.h"
#include "../../src/sync/ConflictResolver.h"
#include "../../src/sync/SyncProtocol.h"
#include "../../src/sync/CommandType.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace lightwaveos {
namespace sync {

//==============================================================================
// DeviceUUID Implementation
//==============================================================================

DeviceUUID& DeviceUUID::instance() {
    static DeviceUUID s_instance;
    return s_instance;
}

DeviceUUID::DeviceUUID() : m_initialized(false) {
    initialize();
}

void DeviceUUID::initialize() {
    if (m_initialized) return;

    // For native build, use a test MAC
    m_mac[0] = 0xDE;
    m_mac[1] = 0xAD;
    m_mac[2] = 0xBE;
    m_mac[3] = 0xEF;
    m_mac[4] = 0x00;
    m_mac[5] = 0x01;

    formatUUID();
    m_initialized = true;
}

void DeviceUUID::formatUUID() {
    snprintf(m_uuidStr, sizeof(m_uuidStr), "LW-%02X%02X%02X%02X%02X%02X",
             m_mac[0], m_mac[1], m_mac[2], m_mac[3], m_mac[4], m_mac[5]);
}

bool DeviceUUID::matches(const char* uuidStr) const {
    return strcmp(m_uuidStr, uuidStr) == 0;
}

bool DeviceUUID::isHigherThan(const uint8_t* other) const {
    for (int i = 0; i < 6; i++) {
        if (m_mac[i] > other[i]) return true;
        if (m_mac[i] < other[i]) return false;
    }
    return false;  // Equal
}

bool DeviceUUID::isHigherThan(const char* otherUuidStr) const {
    uint8_t otherMac[6];
    if (!parseUUID(otherUuidStr, otherMac)) {
        return true;  // Invalid UUID is always lower
    }
    return isHigherThan(otherMac);
}

bool DeviceUUID::parseUUID(const char* uuidStr, uint8_t* outMac) {
    if (!uuidStr || strlen(uuidStr) != 15) return false;
    if (strncmp(uuidStr, "LW-", 3) != 0) return false;

    const char* hex = uuidStr + 3;
    for (int i = 0; i < 6; i++) {
        char byte[3] = { hex[i*2], hex[i*2+1], 0 };
        outMac[i] = (uint8_t)strtol(byte, nullptr, 16);
    }
    return true;
}

//==============================================================================
// LeaderElection Implementation
//==============================================================================

LeaderElection::LeaderElection()
    : m_role(SyncRole::UNKNOWN)
{
    m_leaderUuid[0] = '\0';
}

SyncRole LeaderElection::evaluate(const char* const* connectedPeerUuids, uint8_t peerCount) {
    // Bully algorithm: highest UUID wins
    const char* ourUuid = DEVICE_UUID.toString();
    bool weAreHighest = true;
    const char* highestUuid = ourUuid;

    for (uint8_t i = 0; i < peerCount && connectedPeerUuids != nullptr; i++) {
        if (connectedPeerUuids[i] == nullptr) continue;

        if (!DEVICE_UUID.isHigherThan(connectedPeerUuids[i])) {
            weAreHighest = false;
        }

        // Track highest UUID seen
        uint8_t peerMac[6], highestMac[6];
        if (DeviceUUID::parseUUID(connectedPeerUuids[i], peerMac) &&
            DeviceUUID::parseUUID(highestUuid, highestMac)) {
            bool peerHigher = false;
            for (int j = 0; j < 6; j++) {
                if (peerMac[j] > highestMac[j]) { peerHigher = true; break; }
                if (peerMac[j] < highestMac[j]) break;
            }
            if (peerHigher) {
                highestUuid = connectedPeerUuids[i];
            }
        }
    }

    if (weAreHighest) {
        m_role = SyncRole::LEADER;
        strncpy(m_leaderUuid, ourUuid, sizeof(m_leaderUuid) - 1);
    } else {
        m_role = SyncRole::FOLLOWER;
        strncpy(m_leaderUuid, highestUuid, sizeof(m_leaderUuid) - 1);
    }
    m_leaderUuid[sizeof(m_leaderUuid) - 1] = '\0';

    return m_role;
}

SyncRole LeaderElection::evaluate(const char (*connectedPeerUuids)[16], uint8_t peerCount) {
    // Convert to pointer array and delegate
    const char* ptrs[MAX_PEER_CONNECTIONS];
    for (uint8_t i = 0; i < peerCount && i < MAX_PEER_CONNECTIONS; i++) {
        ptrs[i] = connectedPeerUuids[i];
    }
    return evaluate(ptrs, peerCount);
}

//==============================================================================
// CommandSerializer Implementation
//==============================================================================

size_t CommandSerializer::serializeSetEffect(
    uint8_t effectId,
    uint32_t version,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"eff\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{\"e\":%u}}",
        version, (unsigned)(version % 100000), senderUuid, effectId);
}

size_t CommandSerializer::serializeSetBrightness(
    uint8_t brightness,
    uint32_t version,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"bri\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{\"b\":%u}}",
        version, (unsigned)(version % 100000), senderUuid, brightness);
}

size_t CommandSerializer::serializeSetSpeed(
    uint8_t speed,
    uint32_t version,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"spd\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{\"s\":%u}}",
        version, (unsigned)(version % 100000), senderUuid, speed);
}

size_t CommandSerializer::serializeSetPalette(
    uint8_t paletteId,
    uint32_t version,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"pal\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{\"p\":%u}}",
        version, (unsigned)(version % 100000), senderUuid, paletteId);
}

size_t CommandSerializer::serializeZoneSetEffect(
    uint8_t zoneId,
    uint8_t effectId,
    uint32_t version,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"zef\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{\"z\":%u,\"e\":%u}}",
        version, (unsigned)(version % 100000), senderUuid, zoneId, effectId);
}

size_t CommandSerializer::serializeSetZoneMode(
    bool enabled,
    uint8_t zoneCount,
    uint32_t version,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"zmm\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{\"n\":%d,\"c\":%u}}",
        version, (unsigned)(version % 100000), senderUuid, enabled ? 1 : 0, zoneCount);
}

size_t CommandSerializer::serializeSetVisualParams(
    uint8_t intensity,
    uint8_t saturation,
    uint8_t complexity,
    uint8_t variation,
    uint32_t version,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"vps\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{\"i\":%u,\"a\":%u,\"x\":%u,\"r\":%u}}",
        version, (unsigned)(version % 100000), senderUuid, intensity, saturation, complexity, variation);
}

size_t CommandSerializer::serialize(
    CommandType type,
    uint32_t version,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize,
    const void* params
) {
    (void)params; // Unused in this simplified implementation
    const char* code = commandTypeToCode(type);
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"%s\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{}}",
        code, version, (unsigned)(version % 100000), senderUuid);
}

// Helper to find substring position
static const char* findString(const char* haystack, const char* needle) {
    return strstr(haystack, needle);
}

// Helper to parse integer from JSON
static uint32_t parseUint(const char* json, const char* key) {
    char searchKey[32];
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    const char* pos = findString(json, searchKey);
    if (!pos) return 0;
    pos += strlen(searchKey);
    while (*pos == ' ') pos++;
    return (uint32_t)atoi(pos);
}

// Helper to parse string from JSON
static bool parseString(const char* json, const char* key, char* out, size_t outSize) {
    char searchKey[32];
    snprintf(searchKey, sizeof(searchKey), "\"%s\":\"", key);
    const char* pos = findString(json, searchKey);
    if (!pos) return false;
    pos += strlen(searchKey);
    size_t i = 0;
    while (*pos && *pos != '"' && i < outSize - 1) {
        out[i++] = *pos++;
    }
    out[i] = '\0';
    return true;
}

ParsedCommand CommandSerializer::parse(const char* json, size_t length) {
    ParsedCommand cmd;
    cmd.valid = false;
    memset(&cmd, 0, sizeof(cmd));

    if (!json || length == 0) return cmd;

    // Check message type
    if (!findString(json, "\"t\":\"sync.cmd\"")) return cmd;

    // Parse command code
    char code[8] = {0};
    if (!parseString(json, "c", code, sizeof(code))) return cmd;

    cmd.type = codeToCommandType(code);
    if (cmd.type == CommandType::UNKNOWN) return cmd;

    // Parse version and timestamp
    cmd.version = parseUint(json, "v");
    cmd.timestamp = parseUint(json, "ts");

    // Parse sender UUID
    parseString(json, "u", cmd.senderUuid, sizeof(cmd.senderUuid));

    // Parse params based on command type
    const char* params = findString(json, "\"p\":{");
    if (params) {
        switch (cmd.type) {
            case CommandType::SET_EFFECT:
                cmd.params.effect.effectId = (uint8_t)parseUint(params, "e");
                break;
            case CommandType::SET_BRIGHTNESS:
                cmd.params.brightness.brightness = (uint8_t)parseUint(params, "b");
                break;
            case CommandType::SET_SPEED:
                cmd.params.speed.speed = (uint8_t)parseUint(params, "s");
                break;
            case CommandType::SET_PALETTE:
                cmd.params.palette.paletteId = (uint8_t)parseUint(params, "p");
                break;
            case CommandType::ZONE_SET_EFFECT:
                cmd.params.zoneEffect.zoneId = (uint8_t)parseUint(params, "z");
                cmd.params.zoneEffect.effectId = (uint8_t)parseUint(params, "e");
                break;
            default:
                break;
        }
    }

    cmd.valid = true;
    return cmd;
}

state::ICommand* CommandSerializer::createCommand(const ParsedCommand& parsed) {
    // Simplified - just return nullptr for native tests
    (void)parsed;
    return nullptr;
}

size_t CommandSerializer::writeEnvelopeStart(
    char* buffer,
    size_t bufferSize,
    const char* code,
    uint32_t version,
    const char* uuid
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"%s\",\"v\":%u,\"ts\":%u,\"u\":\"%s\",\"p\":{",
        code, version, (unsigned)(version % 100000), uuid);
}

size_t CommandSerializer::writeEnvelopeEnd(char* buffer, size_t remaining) {
    return snprintf(buffer, remaining, "}}");
}

CommandType codeToCommandType(const char* code) {
    if (!code) return CommandType::UNKNOWN;
    if (strcmp(code, "eff") == 0) return CommandType::SET_EFFECT;
    if (strcmp(code, "bri") == 0) return CommandType::SET_BRIGHTNESS;
    if (strcmp(code, "spd") == 0) return CommandType::SET_SPEED;
    if (strcmp(code, "pal") == 0) return CommandType::SET_PALETTE;
    if (strcmp(code, "zef") == 0) return CommandType::ZONE_SET_EFFECT;
    if (strcmp(code, "zpa") == 0) return CommandType::ZONE_SET_PALETTE;
    if (strcmp(code, "zbr") == 0) return CommandType::ZONE_SET_BRIGHTNESS;
    if (strcmp(code, "zsp") == 0) return CommandType::ZONE_SET_SPEED;
    if (strcmp(code, "zen") == 0) return CommandType::ZONE_ENABLE;
    if (strcmp(code, "zmm") == 0) return CommandType::SET_ZONE_MODE;
    if (strcmp(code, "vps") == 0) return CommandType::SET_VISUAL_PARAMS;
    if (strcmp(code, "ttr") == 0) return CommandType::TRIGGER_TRANSITION;
    if (strcmp(code, "utr") == 0) return CommandType::UPDATE_TRANSITION;
    if (strcmp(code, "ctr") == 0) return CommandType::COMPLETE_TRANSITION;
    if (strcmp(code, "hue") == 0) return CommandType::INCREMENT_HUE;
    if (strcmp(code, "int") == 0) return CommandType::SET_INTENSITY;
    if (strcmp(code, "sat") == 0) return CommandType::SET_SATURATION;
    if (strcmp(code, "cpx") == 0) return CommandType::SET_COMPLEXITY;
    if (strcmp(code, "var") == 0) return CommandType::SET_VARIATION;
    return CommandType::UNKNOWN;
}

//==============================================================================
// StateSerializer Implementation
//==============================================================================

size_t StateSerializer::serialize(
    const state::SystemState& state,
    const char* senderUuid,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.state\",\"v\":%u,\"ts\":%u,\"u\":\"%s\","
        "\"e\":%u,\"b\":%u,\"s\":%u,\"p\":%u,"
        "\"zm\":%s,\"zc\":%u}",
        state.version, (unsigned)(state.version % 100000), senderUuid,
        state.currentEffectId, state.brightness, state.speed, state.currentPaletteId,
        state.zoneModeEnabled ? "true" : "false", state.activeZoneCount);
}

bool StateSerializer::parse(const char* json, size_t length, state::SystemState& stateOut) {
    (void)length;
    if (!json) return false;
    if (!findString(json, "\"t\":\"sync.state\"")) return false;

    stateOut.version = parseUint(json, "v");
    stateOut.currentEffectId = (uint8_t)parseUint(json, "e");
    stateOut.brightness = (uint8_t)parseUint(json, "b");
    stateOut.speed = (uint8_t)parseUint(json, "s");
    stateOut.currentPaletteId = (uint8_t)parseUint(json, "p");
    stateOut.activeZoneCount = (uint8_t)parseUint(json, "zc");

    // Parse boolean
    stateOut.zoneModeEnabled = (findString(json, "\"zm\":true") != nullptr);

    return true;
}

bool StateSerializer::isStateMessage(const char* json, size_t length) {
    (void)length;
    return json && findString(json, "\"t\":\"sync.state\"");
}

uint32_t StateSerializer::extractVersion(const char* json, size_t length) {
    (void)length;
    return parseUint(json, "v");
}

bool StateSerializer::extractSenderUuid(const char* json, size_t length, char* uuidOut) {
    (void)length;
    return parseString(json, "u", uuidOut, 16);
}

//==============================================================================
// ConflictResolver Implementation
//==============================================================================

ConflictResolver::ConflictResolver()
{
}

ConflictDecision ConflictResolver::resolveState(
    uint32_t localVersion,
    uint32_t remoteVersion,
    bool isFromLeader
) const {
    return resolveCommand(localVersion, remoteVersion, isFromLeader);
}

ConflictDecision ConflictResolver::resolveCommand(
    uint32_t localVersion,
    uint32_t remoteVersion,
    bool isFromLeader
) const {
    // Check for version divergence
    if (isVersionDivergent(localVersion, remoteVersion)) {
        return ConflictDecision(ConflictResult::RESYNC_NEEDED, "Versions divergent");
    }

    // Higher version wins
    int cmp = compareVersions(localVersion, remoteVersion);
    if (cmp < 0) {
        return ConflictDecision(ConflictResult::ACCEPT_REMOTE, "Remote version higher");
    } else if (cmp > 0) {
        return ConflictDecision(ConflictResult::ACCEPT_LOCAL, "Local version higher");
    } else {
        // Same version - leader wins ties
        if (isFromLeader) {
            return ConflictDecision(ConflictResult::ACCEPT_REMOTE, "Same version, from leader");
        } else {
            return ConflictDecision(ConflictResult::ACCEPT_LOCAL, "Same version, not from leader");
        }
    }
}

bool ConflictResolver::isVersionDivergent(uint32_t v1, uint32_t v2) const {
    return versionDistance(v1, v2) > VERSION_DIVERGENCE_THRESHOLD;
}

int ConflictResolver::compareVersions(uint32_t v1, uint32_t v2) {
    // Handle wrap-around
    if (v1 == v2) return 0;

    // Use signed comparison with wrap-around awareness
    int32_t diff = (int32_t)(v1 - v2);

    // If difference is very large positive, v1 wrapped (v2 is "higher")
    // If difference is very large negative, v2 wrapped (v1 is "higher")
    // Use half the wrap threshold for signed comparison
    const int32_t halfThreshold = (int32_t)(VERSION_WRAP_THRESHOLD / 2);

    if (diff > halfThreshold) {
        return -1;  // v1 wrapped around, v2 is logically higher
    } else if (diff < -halfThreshold) {
        return 1;   // v2 wrapped around, v1 is logically higher
    }

    return (diff > 0) ? 1 : -1;
}

uint32_t ConflictResolver::versionDistance(uint32_t v1, uint32_t v2) {
    if (v1 >= v2) {
        return v1 - v2;
    } else {
        return v2 - v1;
    }
}

// Note: syncStateToString and syncRoleToString are inline in SyncProtocol.h

} // namespace sync
} // namespace lightwaveos
