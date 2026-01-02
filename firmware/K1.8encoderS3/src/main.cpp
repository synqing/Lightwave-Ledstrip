#include <Arduino.h>
#include <M5AtomS3.h>
#include <Wire.h>
#include <M5ROTATE8.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "esp32-hal-i2c.h"
#include "driver/periph_ctrl.h"
#include "soc/periph_defs.h"

// Configuration
#include "config/Config.h"
#include "config/network_config.h"

// Modules
#include "network/WiFiManager.h"
#include "network/WebSocketClient.h"
#include "network/WsMessageRouter.h"
#include "input/EncoderController.h"
#include "ui/DisplayUI.h"
#include "ui/LedFeedback.h"
#include "debug/AgentDebugLog.h"
#include "parameters/ParameterHandler.h"

// Global instances
EncoderController encoderCtrl;
WiFiManager wifiMgr;
WebSocketClient wsClient;
DisplayUI* displayUI = nullptr;
LedFeedback* ledFeedback = nullptr;
ParameterHandler* paramHandler = nullptr;

// Forward declarations
void onEncoderChange(uint8_t index, uint16_t value, bool wasReset);
void onWebSocketMessage(StaticJsonDocument<512>& doc);
void updateConnectionStatus();
void forceWatchdogReset(const char* reason);

// Hardware-level I2C0 peripheral reset: delete ESP-IDF driver + reset hardware peripheral module
// This is the most aggressive software-only recovery available (short of a full MCU reset).
// Call this when software bus clearing and Wire.end()/begin() cycles are insufficient.
// @param sda_pin SDA pin number (for verification)
// @param scl_pin SCL pin number (for verification)
static void resetI2C0Hardware(uint8_t sda_pin, uint8_t scl_pin) {
    Serial.println("  [HARDWARE RESET] Resetting I2C0 at hardware level...");
    
    // Multiple Wire.end() cycles to release Arduino HAL
    Wire.end();
    delay(50);
    Wire.end();  // Second cycle
    delay(50);
    
    // Verify pins are released
    pinMode(sda_pin, INPUT_PULLUP);
    pinMode(scl_pin, INPUT_PULLUP);
    delay(10);
    
    // Delete ESP-IDF I2C driver for I2C0 (Wire = bus 0)
    // This releases the driver resources and stops the hardware peripheral
    esp_err_t err = i2cDeinit(0);  // 0 = I2C0 = Wire
    if (err != ESP_OK) {
        Serial.printf("  [HARDWARE RESET] i2cDeinit(0) returned: %d (may be OK if not initialized)\n", err);
    } else {
        Serial.println("  [HARDWARE RESET] I2C0 driver deleted successfully");
    }
    delay(50);
    
    // Reset the I2C0 hardware peripheral module at the SOC level
    // This resets all internal state, registers, and FSM of the I2C0 peripheral
    periph_module_reset(PERIPH_I2C0_MODULE);
    Serial.println("  [HARDWARE RESET] I2C0 peripheral module reset (hardware reset)");
    delay(100);  // Critical: allow hardware module time to fully reset
    
    // Additional delay for peripheral to settle after hardware reset
    delay(100);
}

// Attempt to clear a potentially-stuck I2C bus (SDA held low, etc.)
// Uses SCL pulsing + STOP condition as per common I2C recovery practice.
// Enhanced with multiple cycles and more aggressive recovery.
// @param cycles Number of bus clear cycles to perform (default: 2, for post-reset recovery use 3)
static void i2cBusClear(uint8_t sda_pin, uint8_t scl_pin, int cycles = 2) {
    for (int cycle = 0; cycle < cycles; cycle++) {
        // Ensure Wire isn't driving the pins
        Wire.end();
        delay(5);

        pinMode(sda_pin, INPUT_PULLUP);
        pinMode(scl_pin, INPUT_PULLUP);
        delay(2);

        // If SDA is stuck low, try to clock it free (more aggressive: 18 pulses instead of 9)
        if (digitalRead(sda_pin) == LOW) {
            pinMode(scl_pin, OUTPUT_OPEN_DRAIN);
            digitalWrite(scl_pin, HIGH);
            delayMicroseconds(5);

            // More aggressive: 18 pulses (was 9)
            for (int i = 0; i < 18; i++) {
                digitalWrite(scl_pin, LOW);
                delayMicroseconds(5);
                digitalWrite(scl_pin, HIGH);
                delayMicroseconds(5);
            }

            pinMode(scl_pin, INPUT_PULLUP);
            delay(2);
        }

        // Send STOP sequence multiple times (2-3 times per cycle)
        for (int stop_count = 0; stop_count < 2; stop_count++) {
            pinMode(sda_pin, OUTPUT_OPEN_DRAIN);
            pinMode(scl_pin, OUTPUT_OPEN_DRAIN);
            digitalWrite(sda_pin, LOW);
            digitalWrite(scl_pin, HIGH);
            delayMicroseconds(5);
            digitalWrite(sda_pin, HIGH);
            delayMicroseconds(5);

            pinMode(sda_pin, INPUT_PULLUP);
            pinMode(scl_pin, INPUT_PULLUP);
            delay(2);
        }

        // Verify SDA release after each cycle
        pinMode(sda_pin, INPUT_PULLUP);
        pinMode(scl_pin, INPUT_PULLUP);
        delay(5);
        
        // If SDA is still stuck low and we have more cycles, try again
        if (cycle < cycles - 1 && digitalRead(sda_pin) == LOW) {
            delay(10);  // Extra delay before next cycle
        }
    }
    
    // Final verification: ensure SDA is released
    pinMode(sda_pin, INPUT_PULLUP);
    pinMode(scl_pin, INPUT_PULLUP);
    delay(5);
}

// Wake-up sequence: send dummy I2C transactions to try to "wake" stuck encoder
static void i2cWakeUpSequence(uint8_t sda_pin, uint8_t scl_pin, uint32_t freq) {
    // Multiple dummy start/stop sequences
    for (int i = 0; i < 3; i++) {
        Wire.beginTransmission(0x00);  // General call address (may wake I2C devices)
        Wire.endTransmission();
        delay(2);
    }
    
    // Try the target address multiple times
    for (int i = 0; i < 3; i++) {
        Wire.beginTransmission(I2C::ROTATE8_ADDRESS);
        Wire.endTransmission();
        delay(2);
    }
}

// Fast check for M5ROTATE8 at 0x41 - no full scan, just check target address
// @param bus_clear_cycles Number of bus clear cycles (default: 2, for post-reset use 3)
static bool checkForRotate8(uint8_t sda_pin, uint8_t scl_pin, uint32_t freq, int bus_clear_cycles = 2) {
    // Single Wire.end() to release driver (reduced from multiple cycles)
    Wire.end();
    delay(5);  // Reduced from 10ms
    
    // Verify pins are released (should be HIGH with pullups)
    pinMode(sda_pin, INPUT_PULLUP);
    pinMode(scl_pin, INPUT_PULLUP);
    delay(1);  // Reduced from 2ms
    
    // Properly initialize Wire before any operations (with aggressive bus clear)
    i2cBusClear(sda_pin, scl_pin, bus_clear_cycles);
    
    // Reduced delay after bus clear (was 200ms, too long)
    delay(50);  // Reduced from 200ms
    
    // Single Wire.end() before begin (reduced from multiple cycles)
    Wire.end();
    delay(5);  // Reduced from 10ms
    
    Wire.begin(sda_pin, scl_pin, freq);
    Wire.setTimeOut(I2C::TIMEOUT_MS);
    delay(50);  // Reduced from 150ms - I2C bus should stabilize quickly
    
    // Simplified wake-up sequence (fewer iterations, faster)
    Wire.beginTransmission(0x00);  // General call address
    Wire.endTransmission();
    delay(1);
    Wire.beginTransmission(I2C::ROTATE8_ADDRESS);  // Target address
    Wire.endTransmission();
    delay(10);  // Reduced from 50ms
    
    // Direct check for 0x41 only - no full scan
    Wire.beginTransmission(I2C::ROTATE8_ADDRESS);
    uint8_t error = Wire.endTransmission();
    
    return (error == 0);
}

// Multi-rate probe: try 100kHz first, then 50kHz, then 25kHz if that fails
// @param bus_clear_cycles Number of bus clear cycles (default: 2, for post-reset use 3)
static bool checkForRotate8MultiRate(uint8_t sda_pin, uint8_t scl_pin, int bus_clear_cycles = 2) {
    // Try 100kHz first (standard rate)
    if (checkForRotate8(sda_pin, scl_pin, 100000, bus_clear_cycles)) {
        return true;
    }
    
    // Try 50kHz (slower rate, sometimes works better after brownouts/stuck bus)
    Serial.printf("  100kHz probe failed, trying 50kHz... ");
    if (checkForRotate8(sda_pin, scl_pin, 50000, bus_clear_cycles)) {
        Serial.println("found at 50kHz");
        return true;
    }
    
    // Try 25kHz (very slow, last resort for severely stuck bus)
    Serial.printf("50kHz probe failed, trying 25kHz... ");
    if (checkForRotate8(sda_pin, scl_pin, 25000, bus_clear_cycles)) {
        Serial.println("found at 25kHz");
        return true;
    }
    
    Serial.println("not found at any frequency");
    return false;
}

// Attempt M5ROTATE8 initialization with retry logic and multi-rate support
// Returns true if successful, false if failed
// @param bus_clear_cycles Number of bus clear cycles (default: 2, for post-reset use 3)
static bool attemptRotate8Init(uint8_t sda_pin, uint8_t scl_pin, uint32_t freq, bool verbose, int bus_clear_cycles = 2) {
    // Multiple Wire.end() cycles to ensure driver is fully released
    Wire.end();
    delay(50);
    Wire.end();  // Second cycle
    delay(50);
    
    // Verify pins are released
    pinMode(sda_pin, INPUT_PULLUP);
    pinMode(scl_pin, INPUT_PULLUP);
    delay(2);
    
    // Hard reset + aggressive bus clear
    i2cBusClear(sda_pin, scl_pin, bus_clear_cycles);
    
    // Additional delay after bus clear
    delay(200);
    
    // Multiple Wire.end() cycles again before begin
    Wire.end();
    delay(50);
    
    // Initialize Wire with the specified frequency
    Wire.begin(sda_pin, scl_pin, freq);
    Wire.setTimeOut(I2C::TIMEOUT_MS);
    
    // Critical: Wait for I2C bus to stabilize after initialization
    delay(200);  // Longer delay before first I2C operation
    
    // Wake-up sequence
    i2cWakeUpSequence(sda_pin, scl_pin, freq);
    delay(50);
    
    // Flush I2C bus by attempting a quick probe (this clears any stuck state)
    Wire.beginTransmission(I2C::ROTATE8_ADDRESS);
    Wire.endTransmission();
    delay(50);  // Additional delay after flush
    
    // Try to initialize encoder controller
    if (encoderCtrl.begin()) {
        if (verbose) {
            Serial.printf("M5ROTATE8 initialized successfully at %ukHz!\n", (unsigned)(freq / 1000));
        }
        return true;
    }
    
    return false;
}

// Multi-rate initialization: try 100kHz then 50kHz then 25kHz
// @param bus_clear_cycles Number of bus clear cycles (default: 2, for post-reset use 3)
static bool attemptRotate8InitMultiRate(uint8_t sda_pin, uint8_t scl_pin, bool verbose, int bus_clear_cycles = 2) {
    // Try 100kHz first
    if (attemptRotate8Init(sda_pin, scl_pin, 100000, verbose, bus_clear_cycles)) {
        return true;
    }
    
    // Try 50kHz if 100kHz failed
    if (verbose) {
        Serial.println("100kHz init failed, trying 50kHz...");
    }
    if (attemptRotate8Init(sda_pin, scl_pin, 50000, verbose, bus_clear_cycles)) {
        return true;
    }
    
    // Try 25kHz as last resort
    if (verbose) {
        Serial.println("50kHz init failed, trying 25kHz (last resort)...");
    }
    if (attemptRotate8Init(sda_pin, scl_pin, 25000, verbose, bus_clear_cycles)) {
        return true;
    }
    
    return false;
}

// Force watchdog-triggered hard reset when encoder cannot be initialized
void forceWatchdogReset(const char* reason) {
    Serial.println("\n=== ENCODER INITIALIZATION FAILED ===");
    Serial.printf("Reason: %s\n", reason);
    Serial.println("M5ROTATE8 (0x41) is required for K1.8encoderS3 operation.");
    Serial.println("Forcing watchdog reset...");
    Serial.flush();
    delay(500);  // Give serial time to flush
    
    // Initialize task watchdog with 1 second timeout and panic enabled
    // This will trigger a hard reset if we stop feeding the watchdog
    esp_task_wdt_init(1, true);  // 1 second timeout, panic enabled
    
    // Add current task to watchdog
    esp_task_wdt_add(NULL);
    
    // Stop feeding watchdog - this will trigger reset after ~1 second
    Serial.println("Watchdog configured. Stopping watchdog feed to trigger reset...");
    Serial.flush();
    delay(100);
    
    // Infinite loop - watchdog will reset us
    while (true) {
        delay(1000);
        // Intentionally NOT calling esp_task_wdt_reset() here
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n=== K1.8encoderS3 Starting ===");

    // Initialize M5AtomS3
    auto cfg = M5.config();
    AtomS3.begin(cfg);
    Serial.println("AtomS3 initialized");
    
    // Extended settle time: Wait for M5 stack I2C initialization + let encoder firmware "wake up"
    // Increased from 200ms to 750ms for post-reset recovery
    delay(750);

    // CRITICAL: M5Unified initializes I2C0 during AtomS3.begin(), which can leave it in a bad state
    // Especially after firmware upload when encoder stays powered. Reset it IMMEDIATELY.
    Serial.println("Resetting I2C0 hardware (M5Unified may have left it in bad state)...");
    resetI2C0Hardware(2, 1);  // Reset I2C0 before first probe attempt
    delay(100);  // Additional settle after hardware reset

    // Initialize display UI (we still want local visibility while waiting for encoders)
    displayUI = new DisplayUI(AtomS3.Display);
    displayUI->begin();

    // Initialise default values for display
    uint8_t defaultValues[8] = {0, 128, 0, 25, 128, 255, 128, 0};
    displayUI->updateAll(defaultValues);
    Serial.println("Display UI initialized");

    // Encoder is the top priority: do NOT proceed until M5ROTATE8 is detected and initialised.
    // If encoder cannot be brought up within time window, force watchdog reset.
    // Escalating retry strategy: first attempt (normal), second (more aggressive), third (maximum)
    Serial.println("Checking for M5ROTATE8 (0x41)...");
    
    // Candidate bus configurations: Grove port first, then internal
    struct BusCandidate {
        uint8_t sda;
        uint8_t scl;
        const char* name;
    };
    
    BusCandidate candidates[] = {
        {2, 1, "Grove port"},
        {38, 39, "Internal bus"}
    };
    
    uint8_t selected_sda = 0;
    uint8_t selected_scl = 0;
    bool encoderInitialized = false;

    // Bounded time window: 30 seconds total (increased to allow for slower multi-frequency probes)
    const uint32_t MAX_ENCODER_INIT_TIME_MS = 30000;
    uint32_t encoder_init_start_ms = millis();
    uint32_t backoff_ms = 250;
    int attempt_count = 0;
    int retry_level = 0;  // 0=normal, 1=aggressive, 2=maximum

    while (!encoderInitialized) {
        attempt_count++;
        
        // Check if we've exceeded time window
        uint32_t elapsed_ms = millis() - encoder_init_start_ms;
        if (elapsed_ms >= MAX_ENCODER_INIT_TIME_MS) {
            char reason[128];
            snprintf(reason, sizeof(reason), 
                     "Timeout after %lu ms (%d attempts, retry_level=%d)", 
                     (unsigned long)elapsed_ms, attempt_count, retry_level);
            forceWatchdogReset(reason);
            // Will not return - forceWatchdogReset() triggers reset
        }

        // Escalating aggressiveness: determine bus_clear_cycles based on retry level
        int bus_clear_cycles = 2;  // Normal
        if (retry_level == 1) {
            bus_clear_cycles = 3;  // More aggressive
        } else if (retry_level >= 2) {
            bus_clear_cycles = 4;  // Maximum
        }

        // On retry attempts (after first failure), re-apply hardware reset
        if (attempt_count > 1) {
            Serial.println("  [RETRY RECOVERY] Performing hardware-level I2C0 peripheral reset...");
            resetI2C0Hardware(2, 1);
            delay(100);  // Settle time after hardware reset
        }

        bool found_bus = false;

        // Multi-pass bring-up: bus-clear → probe 0x41 (multi-rate) → init attempt
        for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
            if (retry_level > 0) {
                Serial.printf("[Attempt %d, Level %d] Checking %s (SDA=%d, SCL=%d)... ",
                              attempt_count, retry_level, candidates[i].name, candidates[i].sda, candidates[i].scl);
            } else {
                Serial.printf("[Attempt %d] Checking %s (SDA=%d, SCL=%d)... ",
                              attempt_count, candidates[i].name, candidates[i].sda, candidates[i].scl);
            }

            // Multi-rate probe: try 100kHz then 50kHz then 25kHz (with escalating bus clear cycles)
            bool found = checkForRotate8MultiRate(candidates[i].sda, candidates[i].scl, bus_clear_cycles);

            char data[128];
            snprintf(data, sizeof(data),
                     "{\"attempt\":%d,\"retry_level\":%d,\"bus\":\"%s\",\"sda\":%u,\"scl\":%u,\"found_0x41\":%s,\"bus_clear_cycles\":%d}",
                     attempt_count,
                     retry_level,
                     candidates[i].name,
                     (unsigned)candidates[i].sda,
                     (unsigned)candidates[i].scl,
                     found ? "true" : "false",
                     bus_clear_cycles);
            agent_dbg_log("H1", "src/main.cpp:setup", "M5ROTATE8 check result", data);

            if (found) {
                Serial.println("FOUND!");
                selected_sda = candidates[i].sda;
                selected_scl = candidates[i].scl;
                found_bus = true;
                break;
            }
        }

        if (found_bus) {
            Serial.printf("Initializing M5ROTATE8 on SDA=%d, SCL=%d (retry_level=%d)...\n", 
                          selected_sda, selected_scl, retry_level);
            encoderCtrl.setI2CPins(selected_sda, selected_scl);
            
            // Multi-rate initialization: try 100kHz then 50kHz then 25kHz (with escalating bus clear cycles)
            encoderInitialized = attemptRotate8InitMultiRate(selected_sda, selected_scl, true, bus_clear_cycles);
            if (encoderInitialized) {
                encoderCtrl.setChangeCallback(onEncoderChange);
                Serial.println("Encoder controller initialized");
                break;
            } else {
                Serial.printf("Initialization failed after multi-rate probe (retry_level=%d).\n", retry_level);
            }
        }

        // Escalate retry level: after 3 failed attempts, increase aggressiveness
        if (attempt_count % 3 == 0 && retry_level < 2) {
            retry_level++;
            Serial.printf("Escalating to retry level %d (more aggressive recovery)...\n", retry_level);
            delay(500);  // Extra delay before escalating
        }

        Serial.printf("ERROR: Encoders not ready yet (elapsed: %lu ms, level: %d). Retrying...\n", 
                      (unsigned long)(millis() - encoder_init_start_ms), retry_level);
        delay(backoff_ms);
        if (backoff_ms < 2000) backoff_ms += 250;
    }

    // Initialize LED feedback (shares encoder instance with EncoderController)
    ledFeedback = new LedFeedback(encoderCtrl.getEncoder());
    if (ledFeedback->begin()) {
        ledFeedback->setStatus(ConnectionStatus::CONNECTING);
        Serial.println("LED feedback initialized");
    }

    // Now that encoders are live, initialise the higher layers.
    paramHandler = new ParameterHandler(&encoderCtrl, &wsClient, displayUI);
    WsMessageRouter::init(paramHandler);

    // Initialize WiFi
    Serial.printf("Connecting to WiFi: %s\n", NetConfig::SSID);
    wifiMgr.begin(NetConfig::SSID, NetConfig::PASSWORD);
    
    // Set WebSocket message callback
    wsClient.onMessage(onWebSocketMessage);

    Serial.println("=== Setup Complete ===\n");
}

void loop() {
    // Update M5AtomS3
    AtomS3.update();

    // Service WebSocket early in the loop so I2C/encoder work can't starve it.
    // This reduces disconnects caused by missed ping/pong or TCP timeouts.
    wsClient.update();

    // Update encoder polling
    encoderCtrl.update();

    // Update WiFi connection
    wifiMgr.update();

    // Handle WiFi state machine
    if (wifiMgr.isConnected()) {
        // Resolve mDNS if needed (with internal backoff to prevent hammering)
        if (!wifiMgr.isMDNSResolved()) {
            wifiMgr.resolveMDNS("lightwaveos");
        }
        // Configure WebSocket once mDNS is resolved.
        // After initial begin(), WebSocketClient owns reconnection/backoff; do not re-call begin()
        // from the main loop as it bypasses backoff and can thrash the TCP stack.
        static bool wsConfigured = false;
        if (!wsConfigured && wifiMgr.getResolvedIP() != INADDR_NONE) {
            // #region agent log
            {
                char ipBuf[32];
                wifiMgr.getResolvedIP().toString().toCharArray(ipBuf, sizeof(ipBuf));
                char data[220];
                snprintf(
                    data, sizeof(data),
                    "{\"action\":\"begin_once\",\"resolvedIp\":\"%s\",\"wsStatus\":%u,\"reconnectDelayMs\":%lu}",
                    ipBuf,
                    (unsigned)wsClient.getStatus(),
                    (unsigned long)wsClient.getReconnectDelay()
                );
                agent_dbg_log("HwsC", "src/main.cpp:loop", "ws.begin.attempt", data);
            }
            // #endregion
            Serial.printf("Configuring WebSocket to %s\n", wifiMgr.getResolvedIP().toString().c_str());
            wsClient.begin(wifiMgr.getResolvedIP(), 80, "/ws");
            wsConfigured = true;
        }
    }

    // Update connection status LED
    updateConnectionStatus();

    // Update LED animations
    if (ledFeedback) {
        ledFeedback->update();
    }

    // Note: AtomS3 does not have touch. Swipe navigation for future display variant.

    // Periodic status logging for observability (every 10 seconds)
    static unsigned long lastStatusLog = 0;
    unsigned long now = millis();
    if (now - lastStatusLog >= 10000) {
        lastStatusLog = now;
        Serial.printf("[Status] WiFi:%s IP:%s mDNS:%s WS:%s delay:%lu heap:%u\n",
            wifiMgr.isConnected() ? "OK" : "OFF",
            wifiMgr.getLocalIP().toString().c_str(),
            wifiMgr.isMDNSResolved() ? wifiMgr.getResolvedIP().toString().c_str() : "none",
            wsClient.isConnected() ? "OK" : (wsClient.isConnecting() ? "CONN" : "OFF"),
            wsClient.getReconnectDelay(),
            ESP.getFreeHeap());
    }

    delay(10);
}

// Called when encoder value changes
void onEncoderChange(uint8_t index, uint16_t value, bool wasReset) {
    Serial.printf("Encoder %d: %d %s\n", index, value, wasReset ? "(reset)" : "");

    // Flash LED
    if (ledFeedback) {
        ledFeedback->flashEncoder(index);
    }

    // Delegate to parameter handler (handles display update, WebSocket send, validation)
    if (paramHandler) {
        paramHandler->onEncoderChanged(index, value, wasReset);
    }
}

// Called when WebSocket message received
void onWebSocketMessage(StaticJsonDocument<512>& doc) {
    // Route message to appropriate handler via WsMessageRouter
    WsMessageRouter::route(doc);
}

// Update status LED based on connection state
void updateConnectionStatus() {
    static bool lastWsConnected = false;

    if (!ledFeedback) return;

    bool currentWsConnected = wsClient.isConnected();

    if (currentWsConnected != lastWsConnected) {
        if (currentWsConnected) {
            ledFeedback->setStatus(ConnectionStatus::CONNECTED);
            Serial.println("WebSocket connected!");
        } else if (wifiMgr.isConnected()) {
            ledFeedback->setStatus(ConnectionStatus::RECONNECTING);
        } else {
            ledFeedback->setStatus(ConnectionStatus::DISCONNECTED);
        }
        lastWsConnected = currentWsConnected;
    }

    // Update for WiFi state changes
    if (!wifiMgr.isConnected() && !currentWsConnected) {
        ledFeedback->setStatus(ConnectionStatus::CONNECTING);
    }
}

