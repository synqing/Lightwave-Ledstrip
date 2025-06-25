#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "WirelessProtocol.h"

namespace WirelessEncoder {

// Forward declarations
class WirelessTransmitter;
class WirelessReceiver;

// Base class for wireless communication
class WirelessManager {
protected:
    uint8_t deviceMAC[6];
    uint8_t peerMAC[6];
    ConnectionState connectionState = ConnectionState::DISCONNECTED;
    uint16_t sequenceNumber = 0;
    uint32_t lastHeartbeat = 0;
    uint32_t lastPacketTime = 0;
    uint8_t missedPackets = 0;
    DeviceConfig config;
    
    // ESP-NOW callbacks
    static WirelessManager* instance;
    static void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status);
    static void onDataReceived(const uint8_t* mac_addr, const uint8_t* data, int len);
    
public:
    WirelessManager();
    virtual ~WirelessManager();
    
    // Core functions
    virtual bool initialize();
    virtual void update() = 0;
    bool sendPacket(const DataPacket& packet);
    
    // Connection management
    bool pair(const uint8_t* peerAddress);
    void disconnect();
    ConnectionState getConnectionState() const { return connectionState; }
    
    // Configuration
    void setConfig(const DeviceConfig& newConfig) { config = newConfig; }
    DeviceConfig getConfig() const { return config; }
    
protected:
    virtual void handleReceivedPacket(const DataPacket& packet) = 0;
    void updateConnectionState();
    bool addPeer(const uint8_t* peerAddress);
    void removePeer(const uint8_t* peerAddress);
};

// Transmitter implementation (for encoder device)
class WirelessTransmitter : public WirelessManager {
private:
    EncoderData encoderStates[NUM_ENCODERS];
    uint32_t lastUpdateTime = 0;
    uint8_t batteryLevel = 100;
    
    // Encoder value tracking for delta calculation
    int32_t lastEncoderValues[NUM_ENCODERS] = {0};
    uint8_t lastButtonStates[NUM_ENCODERS] = {0};
    uint32_t buttonPressTime[NUM_ENCODERS] = {0};
    
    // Gesture detection
    uint32_t lastDeltaTime[NUM_ENCODERS] = {0};
    int16_t velocityHistory[NUM_ENCODERS][3] = {{0}};
    uint8_t velocityIndex[NUM_ENCODERS] = {0};
    
public:
    WirelessTransmitter();
    
    void update() override;
    void updateEncoderValues(const int32_t* values, const uint8_t* buttons);
    void setBatteryLevel(uint8_t level) { batteryLevel = level; }
    
protected:
    void handleReceivedPacket(const DataPacket& packet) override;
    
private:
    void sendEncoderData();
    void sendHeartbeat();
    void detectGestures(uint8_t encoderId, int16_t delta);
    uint8_t calculateGesture(uint8_t encoderId);
};

// Receiver implementation (for main device)
class WirelessReceiver : public WirelessManager {
private:
    EncoderData latestData[NUM_ENCODERS];
    uint32_t lastDataTime[NUM_ENCODERS] = {0};
    bool dataAvailable[NUM_ENCODERS] = {false};
    
    // Callback functions
    std::function<void(uint8_t, int16_t)> encoderCallback;
    std::function<void(uint8_t, uint8_t)> buttonCallback;
    std::function<void(uint8_t, uint8_t)> gestureCallback;
    std::function<void(uint8_t)> batteryCallback;
    
    // Latency compensation
    struct LatencyBuffer {
        int16_t deltas[3];
        uint32_t timestamps[3];
        uint8_t index = 0;
    } latencyBuffers[NUM_ENCODERS];
    
public:
    WirelessReceiver();
    
    void update() override;
    
    // Get encoder data
    bool getEncoderDelta(uint8_t encoderId, int16_t& delta);
    bool getButtonState(uint8_t encoderId, uint8_t& state);
    bool isEncoderActive(uint8_t encoderId) const;
    
    // Set callbacks
    void setEncoderCallback(std::function<void(uint8_t, int16_t)> cb) { encoderCallback = cb; }
    void setButtonCallback(std::function<void(uint8_t, uint8_t)> cb) { buttonCallback = cb; }
    void setGestureCallback(std::function<void(uint8_t, uint8_t)> cb) { gestureCallback = cb; }
    void setBatteryCallback(std::function<void(uint8_t)> cb) { batteryCallback = cb; }
    
    // Send commands to transmitter
    void sendHapticCommand(uint8_t pattern, uint8_t intensity, uint16_t duration);
    void sendDisplaySync(uint8_t encoderId, const char* paramName, const char* value);
    
protected:
    void handleReceivedPacket(const DataPacket& packet) override;
    
private:
    void processEncoderData(const DataPacket& packet);
    void applyLatencyCompensation(uint8_t encoderId, int16_t& delta, uint32_t timestamp);
};

// Pairing helper class
class WirelessPairing {
private:
    static constexpr uint32_t PAIRING_TIMEOUT = 30000; // 30 seconds
    static constexpr uint8_t PAIRING_CHANNEL = 1;
    
    bool pairingMode = false;
    uint32_t pairingStartTime = 0;
    uint8_t discoveredDevices[10][6];
    uint8_t deviceCount = 0;
    
public:
    bool startPairing();
    void stopPairing();
    bool isPairing() const { return pairingMode; }
    uint8_t getDiscoveredCount() const { return deviceCount; }
    bool getDiscoveredMAC(uint8_t index, uint8_t* mac);
    
    void handlePairingPacket(const uint8_t* mac, const DataPacket& packet);
    
private:
    void broadcastPairingRequest();
    bool isNewDevice(const uint8_t* mac);
};

} // namespace WirelessEncoder