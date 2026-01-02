// ============================================================================
// Tab5.encoder - M5Stack Tab5 (ESP32-P4) Dual ROTATE8 Encoder Controller
// ============================================================================
// Milestone E: Dual M5ROTATE8 Integration (16 Encoders)
//
// This firmware reads 16 rotary encoders via TWO M5ROTATE8 units on the
// same I2C bus (Grove Port.A) using different addresses.
//
// Hardware:
//   - M5Stack Tab5 (ESP32-P4)
//   - M5ROTATE8 Unit A @ 0x42 (reprogrammed via register 0xFF)
//   - M5ROTATE8 Unit B @ 0x41 (factory default)
//   - Both connected to Grove Port.A via hub or daisy-chain
//
// Grove Port.A I2C:
//   - SDA: GPIO 53
//   - SCL: GPIO 54
//   - Unit A: 0x42 (encoders 0-7)
//   - Unit B: 0x41 (encoders 8-15)
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
#include "input/DualEncoderService.h"

// ============================================================================
// I2C Addresses for Dual Unit Setup
// ============================================================================

constexpr uint8_t ADDR_UNIT_A = 0x42;  // Reprogrammed via register 0xFF
constexpr uint8_t ADDR_UNIT_B = 0x41;  // Factory default

// ============================================================================
// Global State
// ============================================================================

// Dual encoder service (initialized in setup)
DualEncoderService* g_encoders = nullptr;

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
            if (addr == ADDR_UNIT_A) {
                Serial.print(" (M5ROTATE8 Unit A)");
            } else if (addr == ADDR_UNIT_B) {
                Serial.print(" (M5ROTATE8 Unit B)");
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
 * @param index Encoder index (0-15)
 * @param value New parameter value
 * @param wasReset true if this was a button-press reset to default
 */
void onEncoderChange(uint8_t index, uint16_t value, bool wasReset) {
    Parameter param = static_cast<Parameter>(index);
    const char* name = getParameterName(param);
    const char* unit = (index < 8) ? "A" : "B";
    uint8_t localIdx = index % 8;

    if (wasReset) {
        Serial.printf("[%s:%d] %s reset to %d\n", unit, localIdx, name, value);
    } else {
        Serial.printf("[%s:%d] %s: → %d\n", unit, localIdx, name, value);
    }

    // TODO (Milestone F): Send to LightwaveOS via WebSocket
    // webSocket.sendParameter(param, value);
}

// ============================================================================
// Display Update (2-column layout for 16 parameters, readable font)
// ============================================================================

void updateDisplay() {
    if (!g_encoders) return;

    // Layout: 2 columns × 8 rows for 16 parameters
    // Column 0: params 0-7 (Unit A)
    // Column 1: params 8-15 (Unit B)
    int yStart = 200;
    int lineHeight = 35;   // 8 rows × 35 = 280px fits well
    int colWidth = 320;    // Half of ~640 usable width

    // BEGIN TRANSACTION - batch all display operations (K1 pattern for flicker-free)
    M5.Display.startWrite();

    M5.Display.setTextSize(2);  // Readable font size!
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    for (uint8_t i = 0; i < 16; i++) {
        int col = i / 8;         // 0 for params 0-7, 1 for params 8-15
        int row = i % 8;         // 0-7 within each column
        int x = 20 + col * colWidth;
        int y = yStart + row * lineHeight;

        Parameter param = static_cast<Parameter>(i);
        const char* name = getParameterName(param);
        uint16_t value = g_encoders->getValue(i);

        // Clear line and redraw (fillRect needed for changing values)
        M5.Display.fillRect(x, y, colWidth - 10, lineHeight - 2, TFT_BLACK);
        M5.Display.setCursor(x, y);
        M5.Display.printf("%2d:%-10s%3d", i, name, value);
    }

    // END TRANSACTION - commit all at once (eliminates flicker)
    M5.Display.endWrite();
}

// ============================================================================
// Status LED Update (consistent green=connected, red=disconnected)
// ============================================================================

void updateStatusLeds() {
    if (!g_encoders) return;

    bool unitA = g_encoders->isUnitAAvailable();
    bool unitB = g_encoders->isUnitBAvailable();

    // Unit A status LED: green if connected, red if not
    g_encoders->setStatusLed(0, unitA ? 0 : 32, unitA ? 32 : 0, 0);

    // Unit B status LED: green if connected, red if not
    g_encoders->setStatusLed(1, unitB ? 0 : 32, unitB ? 32 : 0, 0);
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
    Serial.println("  Tab5.encoder - Milestone E");
    Serial.println("  Dual M5ROTATE8 (16 Encoders)");
    Serial.println("============================================");

    // Initialize M5Stack Tab5
    // This initializes internal I2C (display, touch, audio) automatically
    auto cfg = M5.config();
    M5.begin(cfg);

    // Set display orientation (landscape, USB on left)
    M5.Display.setRotation(3);

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

    // Initialize DualEncoderService with both addresses
    // Unit A @ 0x42 (reprogrammed), Unit B @ 0x41 (factory)
    g_encoders = new DualEncoderService(&Wire, ADDR_UNIT_A, ADDR_UNIT_B);
    g_encoders->setChangeCallback(onEncoderChange);
    bool encoderOk = g_encoders->begin();

    // Check unit status
    bool unitA = g_encoders->isUnitAAvailable();
    bool unitB = g_encoders->isUnitBAvailable();

    Serial.printf("\n[INIT] Unit A (0x%02X): %s\n", ADDR_UNIT_A, unitA ? "OK" : "NOT FOUND");
    Serial.printf("[INIT] Unit B (0x%02X): %s\n", ADDR_UNIT_B, unitB ? "OK" : "NOT FOUND");

    if (unitA && unitB) {
        Serial.println("\n[OK] Both units detected - 16 encoders available!");
        Serial.println("[OK] Milestone E: Dual encoder service active");

        // Flash all LEDs green briefly to indicate success
        g_encoders->transportA().setAllLEDs(0, 64, 0);
        g_encoders->transportB().setAllLEDs(0, 64, 0);
        delay(200);
        g_encoders->allLedsOff();

        // Set status LEDs (both green for connected)
        updateStatusLeds();

        // Show success on Tab5 display (wrapped in transaction for flicker-free)
        M5.Display.startWrite();
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(TFT_GREEN);
        M5.Display.setCursor(20, 20);
        M5.Display.println("Tab5.encoder");
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setCursor(20, 60);
        M5.Display.println("Milestone E: PASS");
        M5.Display.setCursor(20, 100);
        M5.Display.printf("Unit A (0x%02X): OK", ADDR_UNIT_A);
        M5.Display.setCursor(20, 130);
        M5.Display.printf("Unit B (0x%02X): OK", ADDR_UNIT_B);
        M5.Display.endWrite();

        // Show initial values
        updateDisplay();

    } else if (unitA || unitB) {
        // Partial success - one unit available
        Serial.println("\n[WARN] Only one unit detected - 8 encoders available");
        Serial.println("[WARN] Check wiring for missing unit");

        // Set status LEDs (green for available, red for missing)
        updateStatusLeds();

        // Show partial success on display (wrapped in transaction for flicker-free)
        M5.Display.startWrite();
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(TFT_YELLOW);
        M5.Display.setCursor(20, 20);
        M5.Display.println("Tab5.encoder");
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setCursor(20, 60);
        M5.Display.println("Milestone E: PARTIAL");
        M5.Display.setCursor(20, 100);
        M5.Display.setTextColor(unitA ? TFT_GREEN : TFT_RED);
        M5.Display.printf("Unit A (0x%02X): %s", ADDR_UNIT_A, unitA ? "OK" : "FAIL");
        M5.Display.setCursor(20, 130);
        M5.Display.setTextColor(unitB ? TFT_GREEN : TFT_RED);
        M5.Display.printf("Unit B (0x%02X): %s", ADDR_UNIT_B, unitB ? "OK" : "FAIL");
        M5.Display.endWrite();

        // Show initial values
        updateDisplay();

    } else {
        Serial.println("\n[ERROR] No encoder units found!");
        Serial.println("[ERROR] Check wiring:");
        Serial.println("  - Is Unit A (0x42) connected to Grove Port.A?");
        Serial.println("  - Is Unit B (0x41) connected to Grove Port.A?");
        Serial.println("  - Are the Grove cables properly seated?");

        // Show error on Tab5 display (wrapped in transaction for flicker-free)
        M5.Display.startWrite();
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(TFT_RED);
        M5.Display.setCursor(20, 20);
        M5.Display.println("Tab5.encoder");
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setCursor(20, 60);
        M5.Display.println("Milestone E: FAIL");
        M5.Display.setCursor(20, 100);
        M5.Display.println("No encoder units found!");
        M5.Display.setCursor(20, 140);
        M5.Display.println("Check Grove Port.A wiring");
        M5.Display.endWrite();
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
    if (!g_encoders || !g_encoders->isAnyAvailable()) {
        delay(100);
        return;
    }

    // Track if values changed this frame
    static uint16_t lastValues[16] = {0};
    bool anyChange = false;

    // Cache current values before update
    for (uint8_t i = 0; i < 16; i++) {
        lastValues[i] = g_encoders->getValue(i);
    }

    // Update encoder service (polls all 16 encoders, handles debounce, fires callbacks)
    g_encoders->update();

    // Check for changes
    for (uint8_t i = 0; i < 16; i++) {
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

        bool unitA = g_encoders->isUnitAAvailable();
        bool unitB = g_encoders->isUnitBAvailable();

        Serial.printf("[STATUS] A:%s B:%s heap:%u\n",
                      unitA ? "OK" : "FAIL",
                      unitB ? "OK" : "FAIL",
                      ESP.getFreeHeap());

        // Update status LEDs in case connection state changed
        updateStatusLeds();
    }

    delay(5);  // ~200Hz polling
}
