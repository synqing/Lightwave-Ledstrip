#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include <cstddef>

namespace WirelessEncoder {
    constexpr uint8_t PROTOCOL_VERSION = 1;
    constexpr uint32_t MAGIC_NUMBER = 0x4C574553; // "LWES"
    constexpr uint8_t NUM_ENCODERS = 9;
    constexpr uint32_t UPDATE_INTERVAL_US = 10000; // 100Hz
    
    enum class PacketType : uint8_t {
        ENCODER_DATA = 0x01,
        HEARTBEAT = 0x02,
        PAIRING_REQUEST = 0x10,
        PAIRING_RESPONSE = 0x11
    };
    
    struct EncoderData {
        int16_t delta;
        uint8_t button;
        uint8_t gesture;
    } __attribute__((packed));
    
    struct DataPacket {
        uint32_t magic;
        uint8_t version;
        uint8_t type;
        uint8_t deviceId[6];
        uint16_t sequence;
        uint32_t timestamp;
        EncoderData encoders[NUM_ENCODERS];
        uint8_t battery;
        uint16_t crc16;
    } __attribute__((packed));
    
    enum class ConnectionState : uint8_t {
        DISCONNECTED, PAIRING, CONNECTED, RECONNECTING
    };
}