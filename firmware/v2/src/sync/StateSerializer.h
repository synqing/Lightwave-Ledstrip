/**
 * @file StateSerializer.h
 * @brief Serialize SystemState to/from JSON for sync transmission
 *
 * Serializes the complete SystemState (~100 bytes) to compact JSON for
 * full-state synchronization between devices.
 *
 * JSON Format (~450 bytes):
 * {
 *   "t": "sync.state",
 *   "v": 12345,                    // State version
 *   "ts": 98765432,                // Timestamp
 *   "u": "LW-AABBCCDDEEFF",        // Sender UUID
 *   "s": {                         // State object
 *     "e": 5,                      // currentEffectId
 *     "p": 3,                      // currentPaletteId
 *     "b": 200,                    // brightness
 *     "sp": 20,                    // speed
 *     "h": 128,                    // gHue
 *     "i": 200,                    // intensity
 *     "sa": 255,                   // saturation
 *     "cx": 150,                   // complexity
 *     "vr": 100,                   // variation
 *     "zm": true,                  // zoneModeEnabled
 *     "zc": 2,                     // activeZoneCount
 *     "z": [                       // zones array
 *       {"e":1,"p":2,"b":200,"s":15,"n":1},
 *       {"e":3,"p":4,"b":180,"s":20,"n":1},
 *       {"e":0,"p":0,"b":255,"s":15,"n":0},
 *       {"e":0,"p":0,"b":255,"s":15,"n":0}
 *     ],
 *     "ta": false,                 // transitionActive
 *     "tt": 0,                     // transitionType
 *     "tp": 0                      // transitionProgress
 *   }
 * }
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../core/state/SystemState.h"
#include <cstdint>

namespace lightwaveos {
namespace sync {

/**
 * @brief State serializer for full-state sync
 */
class StateSerializer {
public:
    StateSerializer() = default;
    ~StateSerializer() = default;

    /**
     * @brief Serialize complete SystemState to JSON
     *
     * Creates a complete sync.state message with all state data.
     *
     * @param state State to serialize
     * @param senderUuid Sender device UUID
     * @param outBuffer Output buffer for JSON string
     * @param bufferSize Size of output buffer
     * @return Number of bytes written, or 0 on error
     */
    static size_t serialize(
        const state::SystemState& state,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize
    );

    /**
     * @brief Parse a sync.state JSON message into SystemState
     *
     * @param json JSON string to parse
     * @param length Length of JSON string
     * @param outState Output state struct
     * @return true if parsing succeeded
     */
    static bool parse(const char* json, size_t length, state::SystemState& outState);

    /**
     * @brief Check if a JSON message is a state message
     *
     * Quick check without full parsing.
     *
     * @param json JSON string to check
     * @param length Length of JSON string
     * @return true if message type is "sync.state"
     */
    static bool isStateMessage(const char* json, size_t length);

    /**
     * @brief Extract version from state message without full parse
     *
     * @param json JSON string
     * @param length Length of JSON string
     * @return State version, or 0 if not found
     */
    static uint32_t extractVersion(const char* json, size_t length);

    /**
     * @brief Extract sender UUID from state message
     *
     * @param json JSON string
     * @param length Length of JSON string
     * @param outUuid Output buffer (at least 16 bytes)
     * @return true if UUID extracted successfully
     */
    static bool extractSenderUuid(const char* json, size_t length, char* outUuid);

    /**
     * @brief Get estimated serialized size for a state
     *
     * Useful for buffer allocation.
     *
     * @return Estimated size in bytes (~450 for typical state)
     */
    static constexpr size_t estimatedSize() { return 512; }

private:
    /**
     * @brief Serialize a single zone to JSON
     */
    static size_t serializeZone(
        const state::ZoneState& zone,
        char* buffer,
        size_t remaining
    );

    /**
     * @brief Parse a zone object from JSON position
     */
    static bool parseZone(const char* json, state::ZoneState& zone);
};

} // namespace sync
} // namespace lightwaveos
