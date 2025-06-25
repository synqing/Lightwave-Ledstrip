#pragma once

#include <Arduino.h>
#include <esp_now.h>

namespace WirelessEncoder {

// Protocol constants
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint32_t MAGIC_NUMBER = 0x4C574553; // "LWES" - LightWave Encoder System
constexpr uint8_t MAX_PACKET_SIZE = 250;
constexpr uint8_t NUM_ENCODERS = 9; // 8 main + 1 scroll
constexpr uint32_t UPDATE_INTERVAL_US = 10000; // 10ms = 100Hz
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 500;
constexpr uint8_t MAX_MISSED_PACKETS = 3;

// Packet types
enum class PacketType : uint8_t {
    ENCODER_DATA = 0x01,
    HEARTBEAT = 0x02,
    CONFIG_REQUEST = 0x03,
    CONFIG_RESPONSE = 0x04,
    HAPTIC_COMMAND = 0x05,
    DISPLAY_SYNC = 0x06,
    PAIRING_REQUEST = 0x10,
    PAIRING_RESPONSE = 0x11
};

// Encoder data structure (4 bytes per encoder)
struct EncoderData {
    int16_t delta;      // Change since last update
    uint8_t button;     // Button state (0=released, 1=pressed, 2=long press)
    uint8_t gesture;    // Gesture flags (fast spin, acceleration, etc)
} __attribute__((packed));

// Main data packet structure
struct DataPacket {
    // Header (12 bytes)
    uint32_t magic;         // Magic number for validation
    uint8_t version;        // Protocol version
    uint8_t type;           // Packet type
    uint8_t deviceId[6];    // MAC address
    
    // Timing (6 bytes)
    uint16_t sequence;      // Packet sequence number
    uint32_t timestamp;     // Microseconds timestamp
    
    // Payload
    union {
        // Encoder data packet (37 bytes)
        struct {
            EncoderData encoders[NUM_ENCODERS];  // 36 bytes
            uint8_t battery;                      // Battery percentage
        } __attribute__((packed)) encoderData;
        
        // Heartbeat packet (1 byte)
        struct {
            uint8_t status;     // Device status flags
        } __attribute__((packed)) heartbeat;
        
        // Haptic command (4 bytes)
        struct {
            uint8_t pattern;    // Haptic pattern ID
            uint8_t intensity;  // 0-255
            uint16_t duration;  // Duration in ms
        } __attribute__((packed)) haptic;
        
        // Display sync (64 bytes max)
        struct {
            uint8_t encoderId;
            char paramName[32];
            char value[31];
        } __attribute__((packed)) display;
    } payload;
    
    // Footer (2 bytes)
    uint16_t crc16;         // CRC16 checksum
    
} __attribute__((packed));

// Connection state
enum class ConnectionState : uint8_t {
    DISCONNECTED,
    PAIRING,
    CONNECTED,
    RECONNECTING
};

// Device configuration
struct DeviceConfig {
    uint8_t updateRate;         // Hz (10-200)
    uint8_t sensitivity[NUM_ENCODERS];  // Per-encoder sensitivity
    uint8_t ledBrightness;      // 0-255
    bool hapticEnabled;
    uint8_t channel;            // ESP-NOW channel (1-14)
} __attribute__((packed));

// CRC16 calculation
inline uint16_t calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

// Packet validation
inline bool validatePacket(const DataPacket& packet) {
    if (packet.magic != MAGIC_NUMBER) return false;
    if (packet.version != PROTOCOL_VERSION) return false;
    
    // Calculate CRC excluding the CRC field itself
    uint16_t calculatedCRC = calculateCRC16(
        reinterpret_cast<const uint8_t*>(&packet), 
        offsetof(DataPacket, crc16)
    );
    
    return calculatedCRC == packet.crc16;
}

} // namespace WirelessEncoder