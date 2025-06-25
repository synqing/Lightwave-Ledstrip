/*
 * Wireless Encoder Transmitter
 * 
 * Standalone ESP32-S3 sketch for reading 8 encoders + scroll wheel
 * and transmitting wirelessly to the main LED controller device.
 * 
 * Hardware Requirements:
 * - ESP32-S3 DevKit
 * - M5Stack 8-Encoder Unit (I2C: GPIO 13/14)
 * - M5Unit-Scroll (I2C: GPIO 15/21)
 * - LiPo battery with voltage divider on GPIO 36
 * - Optional: Haptic motor on GPIO 25
 * - Optional: Status LEDs on GPIO 26-28
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <FastLED.h>

// Hardware configuration
namespace HardwareConfig {
    // M5Stack 8-Encoder I2C
    constexpr uint8_t I2C_SDA = 13;
    constexpr uint8_t I2C_SCL = 14;
    constexpr uint8_t M5STACK_8ENCODER_ADDR = 0x41;
    
    // M5Unit-Scroll I2C (secondary bus)
    constexpr uint8_t I2C_SDA_SCROLL = 15;
    constexpr uint8_t I2C_SCL_SCROLL = 21;
    constexpr uint8_t M5UNIT_SCROLL_ADDR = 0x40;
    
    // Battery monitoring
    constexpr uint8_t BATTERY_PIN = 36;
    constexpr float VOLTAGE_DIVIDER_RATIO = 2.0f;
    constexpr float BATTERY_MAX_VOLTAGE = 4.2f;
    constexpr float BATTERY_MIN_VOLTAGE = 3.0f;
    
    // Status feedback
    constexpr uint8_t HAPTIC_PIN = 25;
    constexpr uint8_t STATUS_LED_PIN = 26;
    constexpr uint8_t NUM_STATUS_LEDS = 3;
}

// Wireless protocol (simplified version for standalone use)
namespace WirelessProtocol {
    constexpr uint8_t PROTOCOL_VERSION = 1;
    constexpr uint32_t MAGIC_NUMBER = 0x4C574553; // "LWES"
    constexpr uint8_t NUM_ENCODERS = 9;
    constexpr uint32_t UPDATE_INTERVAL_US = 10000; // 100Hz
    constexpr uint32_t HEARTBEAT_INTERVAL_MS = 500;
    
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
    
    // CRC16 calculation
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
}

// Global variables
uint8_t deviceMAC[6];
uint8_t receiverMAC[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}; // UPDATE WITH YOUR RECEIVER MAC
uint16_t sequenceNumber = 0;
bool connected = false;
uint32_t lastHeartbeat = 0;
uint32_t lastUpdate = 0;

// Encoder state
int32_t encoderValues[WirelessProtocol::NUM_ENCODERS] = {0};
int32_t lastEncoderValues[WirelessProtocol::NUM_ENCODERS] = {0};
uint8_t buttonStates[WirelessProtocol::NUM_ENCODERS] = {0};
uint32_t buttonPressTime[WirelessProtocol::NUM_ENCODERS] = {0};

// Battery monitoring
uint8_t batteryPercentage = 100;
uint32_t lastBatteryCheck = 0;

// Status LEDs
CRGB statusLeds[HardwareConfig::NUM_STATUS_LEDS];

// I2C instances
TwoWire mainI2C = TwoWire(0);
TwoWire scrollI2C = TwoWire(1);

// ESP-NOW callback functions
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        statusLeds[0] = CRGB::Green;
    } else {
        statusLeds[0] = CRGB::Red;
        Serial.println("Send failed");
    }
    FastLED.show();
}

void onDataReceived(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (len == sizeof(WirelessProtocol::DataPacket)) {
        WirelessProtocol::DataPacket* packet = (WirelessProtocol::DataPacket*)data;
        
        if (packet->magic == WirelessProtocol::MAGIC_NUMBER) {
            if (packet->type == (uint8_t)WirelessProtocol::PacketType::PAIRING_RESPONSE) {
                connected = true;
                statusLeds[1] = CRGB::Blue;
                Serial.println("✅ Connected to receiver!");
                
                // Haptic feedback for successful connection
                digitalWrite(HardwareConfig::HAPTIC_PIN, HIGH);
                delay(100);
                digitalWrite(HardwareConfig::HAPTIC_PIN, LOW);
            }
        }
    }
}

// Initialize ESP-NOW
bool initESPNOW() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return false;
    }
    
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceived);
    
    // Add receiver as peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverMAC, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return false;
    }
    
    WiFi.macAddress(deviceMAC);
    Serial.print("Transmitter MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", deviceMAC[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
    
    return true;
}

// Read M5Stack 8-Encoder values
void readMainEncoders() {
    mainI2C.beginTransmission(HardwareConfig::M5STACK_8ENCODER_ADDR);
    mainI2C.write(0x00); // Register address
    if (mainI2C.endTransmission() == 0) {
        mainI2C.requestFrom(HardwareConfig::M5STACK_8ENCODER_ADDR, 36); // 8 encoders × 4 bytes + 4 button bytes
        
        // Read encoder values (32-bit each)
        for (int i = 0; i < 8; i++) {
            if (mainI2C.available() >= 4) {
                int32_t value = 0;
                value |= mainI2C.read();
                value |= mainI2C.read() << 8;
                value |= mainI2C.read() << 16;
                value |= mainI2C.read() << 24;
                encoderValues[i] = value;
            }
        }
        
        // Read button states
        for (int i = 0; i < 8; i++) {
            if (mainI2C.available()) {
                buttonStates[i] = mainI2C.read();
            }
        }
    }
}

// Read M5Unit-Scroll value
void readScrollEncoder() {
    scrollI2C.beginTransmission(HardwareConfig::M5UNIT_SCROLL_ADDR);
    scrollI2C.write(0x00);
    if (scrollI2C.endTransmission() == 0) {
        scrollI2C.requestFrom(HardwareConfig::M5UNIT_SCROLL_ADDR, 5); // 4 bytes value + 1 byte button
        
        if (scrollI2C.available() >= 4) {
            int32_t value = 0;
            value |= scrollI2C.read();
            value |= scrollI2C.read() << 8;
            value |= scrollI2C.read() << 16;
            value |= scrollI2C.read() << 24;
            encoderValues[8] = value; // 9th encoder
        }
        
        if (scrollI2C.available()) {
            buttonStates[8] = scrollI2C.read();
        }
    }
}

// Monitor battery level
void updateBattery() {
    uint32_t now = millis();
    if (now - lastBatteryCheck < 5000) return; // Check every 5 seconds
    lastBatteryCheck = now;
    
    // Read battery voltage with averaging
    uint32_t adcSum = 0;
    for (int i = 0; i < 10; i++) {
        adcSum += analogRead(HardwareConfig::BATTERY_PIN);
        delayMicroseconds(100);
    }
    
    float adcVoltage = (adcSum / 10.0f) * (3.3f / 4095.0f);
    float batteryVoltage = adcVoltage * HardwareConfig::VOLTAGE_DIVIDER_RATIO;
    
    // Convert to percentage
    float range = HardwareConfig::BATTERY_MAX_VOLTAGE - HardwareConfig::BATTERY_MIN_VOLTAGE;
    float normalized = (batteryVoltage - HardwareConfig::BATTERY_MIN_VOLTAGE) / range;
    batteryPercentage = constrain(normalized * 100, 0, 100);
    
    // Update status LED based on battery level
    if (batteryPercentage > 50) {
        statusLeds[2] = CRGB::Green;
    } else if (batteryPercentage > 20) {
        statusLeds[2] = CRGB::Yellow;
    } else {
        statusLeds[2] = CRGB::Red;
    }
}

// Send encoder data packet
void sendEncoderData() {
    WirelessProtocol::DataPacket packet;
    
    // Fill header
    packet.magic = WirelessProtocol::MAGIC_NUMBER;
    packet.version = WirelessProtocol::PROTOCOL_VERSION;
    packet.type = (uint8_t)WirelessProtocol::PacketType::ENCODER_DATA;
    memcpy(packet.deviceId, deviceMAC, 6);
    packet.sequence = sequenceNumber++;
    packet.timestamp = micros();
    
    // Fill encoder data with deltas
    for (int i = 0; i < WirelessProtocol::NUM_ENCODERS; i++) {
        int32_t delta = encoderValues[i] - lastEncoderValues[i];
        packet.encoders[i].delta = constrain(delta, -32768, 32767);
        packet.encoders[i].button = buttonStates[i];
        packet.encoders[i].gesture = 0; // Basic implementation
        
        lastEncoderValues[i] = encoderValues[i];
    }
    
    packet.battery = batteryPercentage;
    
    // Calculate CRC
    packet.crc16 = WirelessProtocol::calculateCRC16(
        (uint8_t*)&packet, 
        sizeof(packet) - sizeof(packet.crc16)
    );
    
    // Send packet
    esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)&packet, sizeof(packet));
    if (result != ESP_OK) {
        Serial.printf("Send error: %d\n", result);
    }
}

// Send heartbeat
void sendHeartbeat() {
    WirelessProtocol::DataPacket packet;
    
    packet.magic = WirelessProtocol::MAGIC_NUMBER;
    packet.version = WirelessProtocol::PROTOCOL_VERSION;
    packet.type = (uint8_t)WirelessProtocol::PacketType::HEARTBEAT;
    memcpy(packet.deviceId, deviceMAC, 6);
    packet.sequence = sequenceNumber++;
    packet.timestamp = micros();
    packet.battery = batteryPercentage;
    
    packet.crc16 = WirelessProtocol::calculateCRC16(
        (uint8_t*)&packet, 
        sizeof(packet) - sizeof(packet.crc16)
    );
    
    esp_now_send(receiverMAC, (uint8_t*)&packet, sizeof(packet));
    lastHeartbeat = millis();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Wireless Encoder Transmitter ===");
    Serial.println("ESP32-S3 Wireless Encoder Device");
    
    // Initialize hardware
    pinMode(HardwareConfig::HAPTIC_PIN, OUTPUT);
    pinMode(HardwareConfig::BATTERY_PIN, INPUT);
    analogSetAttenuation(ADC_11db);
    analogReadResolution(12);
    
    // Initialize status LEDs
    FastLED.addLeds<WS2812B, HardwareConfig::STATUS_LED_PIN, GRB>(statusLeds, HardwareConfig::NUM_STATUS_LEDS);
    statusLeds[0] = CRGB::Red;    // Transmit status
    statusLeds[1] = CRGB::Red;    // Connection status
    statusLeds[2] = CRGB::Green;  // Battery status
    FastLED.show();
    
    // Initialize I2C buses
    mainI2C.begin(HardwareConfig::I2C_SDA, HardwareConfig::I2C_SCL, 100000);
    scrollI2C.begin(HardwareConfig::I2C_SDA_SCROLL, HardwareConfig::I2C_SCL_SCROLL, 100000);
    
    Serial.println("I2C buses initialized");
    
    // Initialize ESP-NOW
    if (initESPNOW()) {
        Serial.println("ESP-NOW initialized successfully");
    } else {
        Serial.println("ESP-NOW initialization failed");
        while (1) {
            statusLeds[0] = CRGB::Red;
            statusLeds[1] = CRGB::Red;
            FastLED.show();
            delay(500);
            statusLeds[0] = CRGB::Black;
            statusLeds[1] = CRGB::Black;
            FastLED.show();
            delay(500);
        }
    }
    
    Serial.println("Setup complete. Starting encoder transmission...");
    Serial.printf("Update rate: %d Hz\n", 1000000 / WirelessProtocol::UPDATE_INTERVAL_US);
    Serial.println("Update receiver MAC address in code if needed!");
}

void loop() {
    uint32_t now = micros();
    
    // Update encoder values
    readMainEncoders();
    readScrollEncoder();
    
    // Send encoder data at 100Hz
    if (now - lastUpdate >= WirelessProtocol::UPDATE_INTERVAL_US) {
        lastUpdate = now;
        sendEncoderData();
    }
    
    // Send heartbeat periodically
    if (millis() - lastHeartbeat >= WirelessProtocol::HEARTBEAT_INTERVAL_MS) {
        sendHeartbeat();
    }
    
    // Update battery monitoring
    updateBattery();
    
    // Update status LEDs
    FastLED.show();
    
    // Serial commands for testing
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case 'i':
                Serial.printf("Battery: %d%%, Connected: %s\n", 
                             batteryPercentage, connected ? "Yes" : "No");
                for (int i = 0; i < 9; i++) {
                    Serial.printf("Encoder %d: %ld (btn: %d)\n", i, encoderValues[i], buttonStates[i]);
                }
                break;
            case 'r':
                ESP.restart();
                break;
        }
    }
}