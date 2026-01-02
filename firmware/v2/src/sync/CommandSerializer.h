/**
 * @file CommandSerializer.h
 * @brief Serialize CQRS commands to/from JSON for sync transmission
 *
 * Serializes the 19 CQRS command types to compact JSON format for
 * transmission over WebSocket to synchronized devices.
 *
 * JSON Format:
 * {
 *   "t": "sync.cmd",           // Message type
 *   "c": "eff",                // Command code (3 chars)
 *   "v": 12345,                // State version
 *   "ts": 98765432,            // Timestamp (millis)
 *   "u": "LW-AABBCCDDEEFF",    // Sender UUID
 *   "p": { "e": 5 }            // Parameters (command-specific)
 * }
 *
 * Parameter keys are single characters for compactness:
 * - e: effectId
 * - b: brightness
 * - p: paletteId
 * - s: speed
 * - z: zoneId
 * - n: enabled (boolean as 0/1)
 * - c: zoneCount
 * - t: transitionType
 * - g: progress
 * - i: intensity
 * - a: saturation
 * - x: complexity
 * - r: variation
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "CommandType.h"
#include "../core/state/Commands.h"
#include <cstdint>

namespace lightwaveos {
namespace sync {

/**
 * @brief Parsed command data from JSON
 */
struct ParsedCommand {
    CommandType type;
    uint32_t version;
    uint32_t timestamp;
    char senderUuid[16];

    // Union of all possible parameters
    union {
        struct { uint8_t effectId; } effect;
        struct { uint8_t brightness; } brightness;
        struct { uint8_t paletteId; } palette;
        struct { uint8_t speed; } speed;
        struct { uint8_t zoneId; bool enabled; } zoneEnable;
        struct { uint8_t zoneId; uint8_t effectId; } zoneEffect;
        struct { uint8_t zoneId; uint8_t paletteId; } zonePalette;
        struct { uint8_t zoneId; uint8_t brightness; } zoneBrightness;
        struct { uint8_t zoneId; uint8_t speed; } zoneSpeed;
        struct { bool enabled; uint8_t zoneCount; } zoneMode;
        struct { uint8_t transitionType; } triggerTransition;
        struct { uint8_t transitionType; uint8_t progress; } updateTransition;
        struct { uint8_t intensity; uint8_t saturation; uint8_t complexity; uint8_t variation; } visualParams;
        struct { uint8_t value; } singleParam;  // For intensity, saturation, complexity, variation
    } params;

    bool valid;

    ParsedCommand()
        : type(CommandType::UNKNOWN)
        , version(0)
        , timestamp(0)
        , senderUuid{0}
        , params{}
        , valid(false)
    {}
};

/**
 * @brief Command serializer/deserializer for sync protocol
 */
class CommandSerializer {
public:
    CommandSerializer() = default;
    ~CommandSerializer() = default;

    /**
     * @brief Serialize a command to JSON
     *
     * Creates a complete sync.cmd message with envelope.
     *
     * @param type Command type
     * @param version State version
     * @param senderUuid Sender device UUID
     * @param outBuffer Output buffer for JSON string
     * @param bufferSize Size of output buffer
     * @param params Command-specific parameters (varies by type)
     * @return Number of bytes written, or 0 on error
     */
    static size_t serialize(
        CommandType type,
        uint32_t version,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize,
        const void* params = nullptr
    );

    // Convenience methods for common commands

    /**
     * @brief Serialize SetEffect command
     */
    static size_t serializeSetEffect(
        uint8_t effectId,
        uint32_t version,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize
    );

    /**
     * @brief Serialize SetBrightness command
     */
    static size_t serializeSetBrightness(
        uint8_t brightness,
        uint32_t version,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize
    );

    /**
     * @brief Serialize SetSpeed command
     */
    static size_t serializeSetSpeed(
        uint8_t speed,
        uint32_t version,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize
    );

    /**
     * @brief Serialize SetPalette command
     */
    static size_t serializeSetPalette(
        uint8_t paletteId,
        uint32_t version,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize
    );

    /**
     * @brief Serialize ZoneSetEffect command
     */
    static size_t serializeZoneSetEffect(
        uint8_t zoneId,
        uint8_t effectId,
        uint32_t version,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize
    );

    /**
     * @brief Serialize SetZoneMode command
     */
    static size_t serializeSetZoneMode(
        bool enabled,
        uint8_t zoneCount,
        uint32_t version,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize
    );

    /**
     * @brief Serialize SetVisualParams command
     */
    static size_t serializeSetVisualParams(
        uint8_t intensity,
        uint8_t saturation,
        uint8_t complexity,
        uint8_t variation,
        uint32_t version,
        const char* senderUuid,
        char* outBuffer,
        size_t bufferSize
    );

    /**
     * @brief Parse a sync.cmd JSON message
     *
     * @param json JSON string to parse
     * @param length Length of JSON string
     * @return ParsedCommand with valid=true on success
     */
    static ParsedCommand parse(const char* json, size_t length);

    /**
     * @brief Create an ICommand from parsed command data
     *
     * Allocates a new command object based on the parsed type.
     * Caller is responsible for deleting the returned object.
     *
     * @param parsed Parsed command data
     * @return Pointer to new ICommand, or nullptr on error
     */
    static state::ICommand* createCommand(const ParsedCommand& parsed);

private:
    /**
     * @brief Write JSON envelope start
     */
    static size_t writeEnvelopeStart(
        char* buffer,
        size_t bufferSize,
        const char* code,
        uint32_t version,
        const char* uuid
    );

    /**
     * @brief Write JSON envelope end
     */
    static size_t writeEnvelopeEnd(char* buffer, size_t remaining);
};

} // namespace sync
} // namespace lightwaveos
