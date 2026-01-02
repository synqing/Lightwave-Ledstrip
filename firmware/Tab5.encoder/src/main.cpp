// ============================================================================
// Tab5.encoder - M5Stack Tab5 (ESP32-P4) ROTATE8 Encoder Controller
// ============================================================================
// Milestone D: Full EncoderService Integration
//
// This firmware reads 8 rotary encoders via M5ROTATE8 on Grove Port.A
// and optionally sends parameter changes to LightwaveOS via WebSocket.
//
// Hardware:
//   - M5Stack Tab5 (ESP32-P4)
//   - M5ROTATE8 8-encoder unit connected to Grove Port.A
//
// Grove Port.A I2C:
//   - SDA: GPIO 53
//   - SCL: GPIO 54
//   - Address: 0x41 (M5ROTATE8)
//
// CRITICAL SAFETY NOTE:
// This code does NOT use aggressive I2C recovery (periph_module_reset,
// i2cDeinit, bus clear pin twiddling) because Tab5's internal I2C bus
// is shared with display/touch/audio. The external I2C on Grove Port.A
// is isolated and safe, but we still avoid aggressive recovery to
// maintain system stability.
// ============================================================================

#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>

#include "config/Config.h"
#include "input/EncoderService.h"

// ============================================================================
// Global State
// ============================================================================

// Encoder service (initialized in setup)
EncoderService* g_encoders = nullptr;

// ============================================================================
// I2C Scanner Utility
// ============================================================================

/**
 * Scan the I2C bus and print discovered devices.
 * @param wire The TwoWire instance to scan
 * @param busName Name for logging (e.g., "External I2C")
 * @return Number of devices found
 */
uint8_t scanI2CBus(TwoWire& wire, const char* busName) {
    Serial.printf("\n=== Scanning %s ===\n", busName);
    uint8_t deviceCount = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        wire.beginTransmission(addr);
        uint8_t error = wire.endTransmission();

        if (error == 0) {
            Serial.printf("  Found device at 0x%02X", addr);

            // Identify known devices
            if (addr == I2C::ROTATE8_ADDRESS) {
                Serial.print(" (M5ROTATE8)");
            }

            Serial.println();
            deviceCount++;
        } else if (error == 4) {
            Serial.printf("  Unknown error at 0x%02X\n", addr);
        }
    }

    if (deviceCount == 0) {
        Serial.println("  No devices found!");
    } else {
        Serial.printf("  Total: %d device(s)\n", deviceCount);
    }

    return deviceCount;
}

// ============================================================================
// Encoder Change Callback
// ============================================================================

/**
 * Called when any encoder value changes
 * @param index Encoder index (0-7)
 * @param value New parameter value
 * @param wasReset true if this was a button-press reset to default
 */
void onEncoderChange(uint8_t index, uint16_t value, bool wasReset) {
    Parameter param = static_cast<Parameter>(index);
    const char* name = getParameterName(param);

    if (wasReset) {
        Serial.printf("Encoder %d (%s) button: reset to %d\n", index, name, value);
    } else {
        Serial.printf("Encoder %d (%s): → %d\n", index, name, value);
    }

    // TODO (Milestone E): Send to LightwaveOS via WebSocket
    // webSocket.sendParameter(param, value);
}

// ============================================================================
// Display Update
// ============================================================================

void updateDisplay() {
    if (!g_encoders) return;

    // Start below the header (which ends around y=160)
    int yStart = 200;
    int lineHeight = 40;

    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    // Draw 2 columns of 4 parameters each
    for (uint8_t i = 0; i < 8; i++) {
        int col = i / 4;
        int row = i % 4;
        int x = 20 + col * 300;
        int y = yStart + row * lineHeight;

        Parameter param = static_cast<Parameter>(i);
        const char* name = getParameterName(param);
        uint16_t value = g_encoders->getValue(i);

        // Clear the line area and redraw
        M5.Display.fillRect(x, y, 280, lineHeight - 5, TFT_BLACK);
        M5.Display.setCursor(x, y);
        M5.Display.printf("%d: %-11s %3d", i, name, value);
    }
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    // Initialize serial first for early logging
    Serial.begin(115200);
    delay(100);

    Serial.println("\n");
    Serial.println("============================================");
    Serial.println("  Tab5.encoder - Milestone D");
    Serial.println("  Full EncoderService");
    Serial.println("============================================");

    // Initialize M5Stack Tab5
    // This initializes internal I2C (display, touch, audio) automatically
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.println("\n[INIT] M5Stack Tab5 initialized");

    // Get external I2C pin configuration from M5Unified
    // Tab5 Grove Port.A: SDA=GPIO53, SCL=GPIO54
    int8_t extSDA = M5.Ex_I2C.getSDA();
    int8_t extScl = M5.Ex_I2C.getSCL();

    Serial.printf("[INIT] Tab5 External I2C pins - SDA:%d SCL:%d\n", extSDA, extScl);

    // Verify pins match expected values
    if (extSDA != I2C::EXT_SDA_PIN || extScl != I2C::EXT_SCL_PIN) {
        Serial.println("[WARN] External I2C pins differ from expected!");
        Serial.printf("[WARN] Expected SDA:%d SCL:%d, got SDA:%d SCL:%d\n",
                      I2C::EXT_SDA_PIN, I2C::EXT_SCL_PIN, extSDA, extScl);
    }

    // Initialize Wire on external I2C bus (Grove Port.A)
    // This is ISOLATED from Tab5's internal I2C (display, touch, audio)
    Wire.begin(extSDA, extScl, I2C::FREQ_HZ);
    Wire.setTimeOut(I2C::TIMEOUT_MS);

    Serial.printf("[INIT] Wire initialized at %lu Hz, timeout %u ms\n",
                  (unsigned long)I2C::FREQ_HZ, I2C::TIMEOUT_MS);

    // Allow I2C bus to stabilize
    delay(100);

    // Scan external I2C bus for devices
    uint8_t found = scanI2CBus(Wire, "External I2C (Grove Port.A)");

    // Initialize EncoderService
    g_encoders = new EncoderService(&Wire, I2C::ROTATE8_ADDRESS);
    g_encoders->setChangeCallback(onEncoderChange);
    bool encoderOk = g_encoders->begin();

    if (encoderOk) {
        uint8_t version = g_encoders->getVersion();
        Serial.printf("\n[OK] EncoderService initialized (firmware v%d)\n", version);
        Serial.println("[OK] Milestone D: Full encoder service active");

        // Flash all LEDs green briefly to indicate success
        g_encoders->transport().setAllLEDs(0, 64, 0);
        delay(200);
        g_encoders->allLedsOff();

        // Set status LED (LED 8) to dim green
        g_encoders->setStatusLed(0, 32, 0);

        // Show success on Tab5 display
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(TFT_GREEN);
        M5.Display.setCursor(20, 20);
        M5.Display.println("Tab5.encoder");
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setCursor(20, 60);
        M5.Display.println("Milestone D: PASS");
        M5.Display.setCursor(20, 100);
        M5.Display.printf("ROTATE8 v%d at 0x%02X", version, I2C::ROTATE8_ADDRESS);
        M5.Display.setCursor(20, 140);
        M5.Display.printf("I2C: SDA=%d SCL=%d", extSDA, extScl);

        // Show initial values
        updateDisplay();

    } else {
        Serial.println("\n[ERROR] EncoderService initialization failed!");
        Serial.println("[ERROR] Check wiring:");
        Serial.println("  - Is ROTATE8 connected to Grove Port.A?");
        Serial.println("  - Is the Grove cable properly seated?");
        Serial.println("  - Is ROTATE8 powered?");

        // Show error on Tab5 display
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(TFT_RED);
        M5.Display.setCursor(20, 20);
        M5.Display.println("Tab5.encoder");
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setCursor(20, 60);
        M5.Display.println("Milestone D: FAIL");
        M5.Display.setCursor(20, 100);
        M5.Display.println("EncoderService init failed!");
        M5.Display.setCursor(20, 140);
        M5.Display.println("Check Grove Port.A wiring");
    }

    Serial.println("\n============================================");
    Serial.println("  Setup complete - turn encoders to test");
    Serial.println("============================================\n");
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
    // Update M5Stack (handles button events, touch, etc.)
    M5.update();

    // Skip encoder processing if service not available
    if (!g_encoders || !g_encoders->isAvailable()) {
        delay(100);
        return;
    }

    // Track if values changed this frame
    static uint16_t lastValues[8] = {0};
    bool anyChange = false;

    // Cache current values before update
    for (uint8_t i = 0; i < 8; i++) {
        lastValues[i] = g_encoders->getValue(i);
    }

    // Update encoder service (polls all 8 encoders, handles debounce, fires callbacks)
    g_encoders->update();

    // Check for changes
    for (uint8_t i = 0; i < 8; i++) {
        if (g_encoders->getValue(i) != lastValues[i]) {
            anyChange = true;
            break;
        }
    }

    // Update display if any value changed
    if (anyChange) {
        updateDisplay();
    }

    // Periodic status (every 10 seconds)
    static uint32_t lastStatus = 0;
    uint32_t now = millis();

    if (now - lastStatus >= 10000) {
        lastStatus = now;

        if (g_encoders->isConnected()) {
            Serial.printf("[STATUS] 8Enc:OK heap:%u\n", ESP.getFreeHeap());
        } else {
            Serial.printf("[STATUS] 8Enc:DISCONNECTED heap:%u\n", ESP.getFreeHeap());
            // Update status LED to red
            g_encoders->setStatusLed(64, 0, 0);
        }
    }

    delay(5);  // ~200Hz polling
}
