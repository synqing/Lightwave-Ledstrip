#pragma once

#include "../config/features.h"

#if FEATURE_WIRELESS_ENCODERS

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <functional>
#include "WirelessProtocol.h"

namespace WirelessEncoder {

class SimpleWirelessReceiver {
private:
    uint8_t deviceMAC[6];
    bool initialized = false;
    bool connected = false;
    uint32_t lastPacketTime = 0;
    uint16_t lastSequence = 0;
    
    // Encoder data
    int16_t encoderDeltas[NUM_ENCODERS] = {0};
    uint8_t buttonStates[NUM_ENCODERS] = {0};
    bool dataAvailable[NUM_ENCODERS] = {false};
    uint32_t lastDataTime[NUM_ENCODERS] = {0};
    
    // Callbacks
    std::function<void(uint8_t, int16_t)> encoderCallback;
    std::function<void(uint8_t, uint8_t)> buttonCallback;
    
    // Static instance for ESP-NOW callbacks
    static inline SimpleWirelessReceiver* instance = nullptr;
    
    static void onDataReceived(const uint8_t* mac_addr, const uint8_t* data, int len) {
        if (instance) {
            instance->handleReceivedPacket(mac_addr, data, len);
        }
    }
    
    static void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
        // Not needed for receiver
    }
    
public:
    SimpleWirelessReceiver() {
        instance = this;
        WiFi.macAddress(deviceMAC);
    }
    
    bool initialize() {
        if (initialized) return true;
        
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        
        if (esp_now_init() != ESP_OK) {
            Serial.println("ESP-NOW init failed");
            return false;
        }
        
        esp_now_register_recv_cb(onDataReceived);
        esp_now_register_send_cb(onDataSent);
        
        Serial.print("Wireless Receiver MAC: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", deviceMAC[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.println();
        
        initialized = true;
        return true;
    }
    
    void update() {
        if (!initialized) return;
        
        uint32_t now = millis();
        
        // Check connection timeout
        if (connected && (now - lastPacketTime > 2000)) {
            connected = false;
            Serial.println("Wireless encoder disconnected");
        }
        
        // Mark old data as stale
        for (int i = 0; i < NUM_ENCODERS; i++) {
            if (dataAvailable[i] && (now - lastDataTime[i] > 100)) {
                dataAvailable[i] = false;
            }
        }
    }
    
    bool isConnected() const { return connected; }
    
    bool getEncoderDelta(uint8_t encoderId, int16_t& delta) {
        if (encoderId >= NUM_ENCODERS || !dataAvailable[encoderId]) {
            return false;
        }
        delta = encoderDeltas[encoderId];
        encoderDeltas[encoderId] = 0; // Clear after reading
        return true;
    }
    
    bool getButtonState(uint8_t encoderId, uint8_t& state) {
        if (encoderId >= NUM_ENCODERS || !dataAvailable[encoderId]) {
            return false;
        }
        state = buttonStates[encoderId];
        return true;
    }
    
    void setEncoderCallback(std::function<void(uint8_t, int16_t)> cb) {
        encoderCallback = cb;
    }
    
    void setButtonCallback(std::function<void(uint8_t, uint8_t)> cb) {
        buttonCallback = cb;
    }
    
private:
    void handleReceivedPacket(const uint8_t* mac_addr, const uint8_t* data, int len) {
        if (len != sizeof(DataPacket)) return;
        
        DataPacket* packet = (DataPacket*)data;
        
        // Validate packet
        if (packet->magic != MAGIC_NUMBER) return;
        if (packet->version != PROTOCOL_VERSION) return;
        
        // Validate CRC
        uint16_t receivedCRC = packet->crc16;
        packet->crc16 = 0; // Clear for calculation
        uint16_t calculatedCRC = calculateCRC16((uint8_t*)packet, sizeof(DataPacket) - 2);
        packet->crc16 = receivedCRC; // Restore
        
        if (calculatedCRC != receivedCRC) return;
        
        // Update connection status
        if (!connected) {
            connected = true;
            Serial.println("Wireless encoder connected!");
        }
        lastPacketTime = millis();
        
        // Process packet based on type
        switch ((PacketType)packet->type) {
            case PacketType::ENCODER_DATA:
                processEncoderData(packet);
                break;
                
            case PacketType::HEARTBEAT:
                // Just update connection time
                break;
                
            case PacketType::PAIRING_REQUEST:
                // Send pairing response
                sendPairingResponse(mac_addr);
                break;
                
            default:
                break;
        }
    }
    
    void processEncoderData(DataPacket* packet) {
        uint32_t now = millis();
        
        for (int i = 0; i < NUM_ENCODERS; i++) {
            if (packet->encoders[i].delta != 0 || packet->encoders[i].button != 0) {
                encoderDeltas[i] = packet->encoders[i].delta;
                buttonStates[i] = packet->encoders[i].button;
                dataAvailable[i] = true;
                lastDataTime[i] = now;
                
                // Call callbacks if available
                if (packet->encoders[i].delta != 0 && encoderCallback) {
                    encoderCallback(i, packet->encoders[i].delta);
                }
                
                if (packet->encoders[i].button != 0 && buttonCallback) {
                    buttonCallback(i, packet->encoders[i].button);
                }
            }
        }
    }
    
    void sendPairingResponse(const uint8_t* transmitterMAC) {
        // Add transmitter as peer
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, transmitterMAC, 6);
        peerInfo.channel = 1;
        peerInfo.encrypt = false;
        
        esp_now_del_peer(transmitterMAC); // Remove if exists
        esp_now_add_peer(&peerInfo);
        
        // Send response
        DataPacket response;
        response.magic = MAGIC_NUMBER;
        response.version = PROTOCOL_VERSION;
        response.type = (uint8_t)PacketType::PAIRING_RESPONSE;
        memcpy(response.deviceId, deviceMAC, 6);
        response.sequence = 0;
        response.timestamp = micros();
        response.battery = 0;
        
        response.crc16 = calculateCRC16((uint8_t*)&response, sizeof(DataPacket) - 2);
        
        esp_now_send(transmitterMAC, (uint8_t*)&response, sizeof(DataPacket));
        
        Serial.print("Sent pairing response to: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", transmitterMAC[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.println();
    }
    
    uint16_t calculateCRC16(const uint8_t* data, size_t length) {
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
};

// Static member definition moved to inline declaration above

} // namespace WirelessEncoder

#endif // FEATURE_WIRELESS_ENCODERS