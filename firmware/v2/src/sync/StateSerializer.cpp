/**
 * @file StateSerializer.cpp
 * @brief State serialization implementation
 */

#include "StateSerializer.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef NATIVE_BUILD
#define millis() 0
#else
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace sync {

// Helper functions for JSON parsing (shared with CommandSerializer)
namespace {
    bool findString(const char* json, const char* key, char* out, size_t outSize) {
        const char* keyPos = strstr(json, key);
        if (!keyPos) return false;

        keyPos += strlen(key);
        keyPos = strchr(keyPos, ':');
        if (!keyPos) return false;

        keyPos++;
        while (*keyPos == ' ' || *keyPos == '"') keyPos++;

        size_t i = 0;
        while (*keyPos && *keyPos != '"' && i < outSize - 1) {
            out[i++] = *keyPos++;
        }
        out[i] = '\0';
        return i > 0;
    }

    bool findLong(const char* json, const char* key, long* out) {
        const char* keyPos = strstr(json, key);
        if (!keyPos) return false;

        keyPos += strlen(key);
        keyPos = strchr(keyPos, ':');
        if (!keyPos) return false;

        keyPos++;
        while (*keyPos == ' ') keyPos++;

        char* endPtr;
        *out = strtol(keyPos, &endPtr, 10);
        return endPtr != keyPos;
    }

    bool findBool(const char* json, const char* key, bool* out) {
        const char* keyPos = strstr(json, key);
        if (!keyPos) return false;

        keyPos += strlen(key);
        keyPos = strchr(keyPos, ':');
        if (!keyPos) return false;

        keyPos++;
        while (*keyPos == ' ') keyPos++;

        if (strncmp(keyPos, "true", 4) == 0) {
            *out = true;
            return true;
        } else if (strncmp(keyPos, "false", 5) == 0) {
            *out = false;
            return true;
        } else if (*keyPos == '1') {
            *out = true;
            return true;
        } else if (*keyPos == '0') {
            *out = false;
            return true;
        }

        return false;
    }
}

size_t StateSerializer::serializeZone(
    const state::ZoneState& zone,
    char* buffer,
    size_t remaining
) {
    return snprintf(buffer, remaining,
        "{\"e\":%u,\"p\":%u,\"b\":%u,\"s\":%u,\"n\":%d}",
        zone.effectId,
        zone.paletteId,
        zone.brightness,
        zone.speed,
        zone.enabled ? 1 : 0
    );
}

size_t StateSerializer::serialize(
    const state::SystemState& state,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize
) {
    if (!outBuffer || bufferSize < 128) return 0;

    size_t written = 0;

    // Envelope start
    written = snprintf(outBuffer, bufferSize,
        "{\"t\":\"sync.state\",\"v\":%lu,\"ts\":%lu,\"u\":\"%s\",\"s\":{",
        static_cast<unsigned long>(state.version),
        static_cast<unsigned long>(millis()),
        senderUuid ? senderUuid : ""
    );
    if (written >= bufferSize) return 0;

    // Global parameters
    written += snprintf(outBuffer + written, bufferSize - written,
        "\"e\":%u,\"p\":%u,\"b\":%u,\"sp\":%u,\"h\":%u,",
        state.currentEffectId,
        state.currentPaletteId,
        state.brightness,
        state.speed,
        state.gHue
    );
    if (written >= bufferSize) return 0;

    // Visual parameters
    written += snprintf(outBuffer + written, bufferSize - written,
        "\"i\":%u,\"sa\":%u,\"cx\":%u,\"vr\":%u,",
        state.intensity,
        state.saturation,
        state.complexity,
        state.variation
    );
    if (written >= bufferSize) return 0;

    // Zone mode
    written += snprintf(outBuffer + written, bufferSize - written,
        "\"zm\":%s,\"zc\":%u,\"z\":[",
        state.zoneModeEnabled ? "true" : "false",
        state.activeZoneCount
    );
    if (written >= bufferSize) return 0;

    // Zones array
    for (uint8_t i = 0; i < state::MAX_ZONES; i++) {
        if (i > 0) {
            outBuffer[written++] = ',';
            if (written >= bufferSize) return 0;
        }
        written += serializeZone(state.zones[i], outBuffer + written, bufferSize - written);
        if (written >= bufferSize) return 0;
    }

    // Close zones array, transition state, and envelope
    written += snprintf(outBuffer + written, bufferSize - written,
        "],\"ta\":%s,\"tt\":%u,\"tp\":%u}}",
        state.transitionActive ? "true" : "false",
        state.transitionType,
        state.transitionProgress
    );

    return written;
}

bool StateSerializer::isStateMessage(const char* json, size_t length) {
    (void)length;
    if (!json) return false;

    char msgType[16] = {0};
    if (!findString(json, "\"t\"", msgType, sizeof(msgType))) return false;
    return strcmp(msgType, "sync.state") == 0;
}

uint32_t StateSerializer::extractVersion(const char* json, size_t length) {
    (void)length;
    if (!json) return 0;

    long version = 0;
    if (!findLong(json, "\"v\"", &version)) return 0;
    return static_cast<uint32_t>(version);
}

bool StateSerializer::extractSenderUuid(const char* json, size_t length, char* outUuid) {
    (void)length;
    if (!json || !outUuid) return false;
    return findString(json, "\"u\"", outUuid, 16);
}

bool StateSerializer::parseZone(const char* json, state::ZoneState& zone) {
    long val;

    if (findLong(json, "\"e\"", &val)) zone.effectId = static_cast<uint8_t>(val);
    if (findLong(json, "\"p\"", &val)) zone.paletteId = static_cast<uint8_t>(val);
    if (findLong(json, "\"b\"", &val)) zone.brightness = static_cast<uint8_t>(val);
    if (findLong(json, "\"s\"", &val)) zone.speed = static_cast<uint8_t>(val);
    if (findLong(json, "\"n\"", &val)) zone.enabled = val != 0;

    return true;
}

bool StateSerializer::parse(const char* json, size_t length, state::SystemState& outState) {
    (void)length;
    if (!json) return false;

    // Verify message type
    if (!isStateMessage(json, length)) return false;

    // Find the state object "s":{...}
    const char* stateStart = strstr(json, "\"s\":{");
    if (!stateStart) return false;
    stateStart += 4; // Skip to {

    long val;

    // Parse global parameters
    if (findLong(json, "\"v\":", &val)) outState.version = static_cast<uint32_t>(val);
    if (findLong(stateStart, "\"e\":", &val)) outState.currentEffectId = static_cast<EffectId>(val);
    if (findLong(stateStart, "\"p\":", &val)) outState.currentPaletteId = static_cast<uint8_t>(val);
    if (findLong(stateStart, "\"b\":", &val)) outState.brightness = static_cast<uint8_t>(val);
    if (findLong(stateStart, "\"sp\":", &val)) outState.speed = static_cast<uint8_t>(val);
    if (findLong(stateStart, "\"h\":", &val)) outState.gHue = static_cast<uint8_t>(val);

    // Parse visual parameters
    if (findLong(stateStart, "\"i\":", &val)) outState.intensity = static_cast<uint8_t>(val);
    if (findLong(stateStart, "\"sa\":", &val)) outState.saturation = static_cast<uint8_t>(val);
    if (findLong(stateStart, "\"cx\":", &val)) outState.complexity = static_cast<uint8_t>(val);
    if (findLong(stateStart, "\"vr\":", &val)) outState.variation = static_cast<uint8_t>(val);

    // Parse zone mode
    bool boolVal;
    if (findBool(stateStart, "\"zm\":", &boolVal)) outState.zoneModeEnabled = boolVal;
    if (findLong(stateStart, "\"zc\":", &val)) outState.activeZoneCount = static_cast<uint8_t>(val);

    // Parse zones array
    const char* zonesStart = strstr(stateStart, "\"z\":[");
    if (zonesStart) {
        zonesStart += 5; // Skip to first zone

        for (uint8_t i = 0; i < state::MAX_ZONES; i++) {
            // Find the start of this zone object
            const char* zoneStart = strchr(zonesStart, '{');
            if (!zoneStart) break;

            // Find the end of this zone object
            const char* zoneEnd = strchr(zoneStart, '}');
            if (!zoneEnd) break;

            // Parse this zone (simple since it's a small substring)
            parseZone(zoneStart, outState.zones[i]);

            // Move to next zone
            zonesStart = zoneEnd + 1;
        }
    }

    // Parse transition state
    if (findBool(stateStart, "\"ta\":", &boolVal)) outState.transitionActive = boolVal;
    if (findLong(stateStart, "\"tt\":", &val)) outState.transitionType = static_cast<uint8_t>(val);
    if (findLong(stateStart, "\"tp\":", &val)) outState.transitionProgress = static_cast<uint8_t>(val);

    return true;
}

} // namespace sync
} // namespace lightwaveos
