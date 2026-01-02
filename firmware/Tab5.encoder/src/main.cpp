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
// K1-Style Color Palette (RGB565 Neon Cyberpunk)
// ============================================================================

// Background color (near-black)
constexpr uint16_t COLOR_BG = 0x0841;  // #0a0a14

// Per-parameter colors (each encoder has unique color for identification)
constexpr uint16_t PARAM_COLORS[16] = {
    // Unit A (0-7): Core parameters
    0xF810,  // 0: Effect     - hot pink    #ff0080
    0xFFE0,  // 1: Brightness - yellow      #ffff00
    0x07FF,  // 2: Palette    - cyan        #00ffff
    0xFA20,  // 3: Speed      - orange      #ff4400
    0xF81F,  // 4: Intensity  - magenta     #ff00ff
    0x07F1,  // 5: Saturation - green       #00ff88
    0x901F,  // 6: Complexity - purple      #8800ff
    0x047F,  // 7: Variation  - blue        #0088ff
    // Unit B (8-15): Extended parameters (repeat palette)
    0xF810,  // 8:  Param8    - hot pink
    0xFFE0,  // 9:  Param9    - yellow
    0x07FF,  // 10: Param10   - cyan
    0xFA20,  // 11: Param11   - orange
    0xF81F,  // 12: Param12   - magenta
    0x07F1,  // 13: Param13   - green
    0x901F,  // 14: Param14   - purple
    0x047F,  // 15: Param15   - blue
};

// ============================================================================
// RGB565 Color Manipulation (K1 Pattern)
// ============================================================================

/**
 * Dim an RGB565 color by a factor (0.0 to 1.5+)
 * Used for glow effects: outer=30%, middle=50%, inner=100%
 */
inline uint16_t dimColor(uint16_t color, float factor) {
    // Extract RGB565 components
    uint8_t r = (color >> 11) & 0x1F;  // 5 bits red (0-31)
    uint8_t g = (color >> 5) & 0x3F;   // 6 bits green (0-63)
    uint8_t b = color & 0x1F;          // 5 bits blue (0-31)

    // Scale each component (clamp to max)
    uint8_t rScaled = static_cast<uint8_t>(r * factor);
    uint8_t gScaled = static_cast<uint8_t>(g * factor);
    uint8_t bScaled = static_cast<uint8_t>(b * factor);

    r = (rScaled > 31) ? 31 : rScaled;
    g = (gScaled > 63) ? 63 : gScaled;
    b = (bScaled > 31) ? 31 : bScaled;

    // Reconstruct RGB565
    return (r << 11) | (g << 5) | b;
}

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

// Forward declaration for highlight support
void updateDisplay(int8_t highlightIdx);

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

    // Update display with this encoder highlighted
    updateDisplay(index);

    // TODO (Milestone F): Send to LightwaveOS via WebSocket
    // webSocket.sendParameter(param, value);
}

// ============================================================================
// Display Update (K1-Style: State Caching, Selective Updates, Glow Borders)
// ============================================================================

// State cache for selective updates (K1 pattern: only redraw changed cells)
static uint16_t s_displayValues[16] = {
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
};  // Initialize to impossible values to force first draw
static int8_t s_highlightIndex = -1;  // Currently active encoder (-1 = none)

/**
 * Draw 3-layer glow border around a cell (K1 pattern)
 * Creates soft neon glow effect with 30%, 50%, 100% brightness layers
 */
void drawGlowBorder(int16_t x, int16_t y, int16_t w, int16_t h,
                    uint16_t color, bool highlight) {
    float glow = highlight ? 1.0f : 0.6f;  // Brighter when highlighted

    uint16_t outerColor = dimColor(color, 0.3f * glow);   // 18-30%
    uint16_t middleColor = dimColor(color, 0.5f * glow);  // 30-50%
    uint16_t innerColor = dimColor(color, glow);          // 60-100%

    M5.Display.drawRect(x, y, w, h, outerColor);
    M5.Display.drawRect(x + 1, y + 1, w - 2, h - 2, middleColor);
    M5.Display.drawRect(x + 2, y + 2, w - 4, h - 4, innerColor);
}

/**
 * Draw a single parameter cell with K1 styling
 */
void drawCell(uint8_t index, uint16_t value, bool highlight) {
    // Layout constants
    constexpr int yStart = 200;
    constexpr int cellHeight = 35;
    constexpr int colWidth = 320;
    constexpr int barHeight = 8;
    constexpr int barYOffset = 24;  // Below text

    int col = index / 8;
    int row = index % 8;
    int x = 20 + col * colWidth;
    int y = yStart + row * cellHeight;
    int cellW = colWidth - 20;

    uint16_t paramColor = PARAM_COLORS[index];

    // Clear cell background
    M5.Display.fillRect(x, y, cellW, cellHeight - 2, COLOR_BG);

    // Draw glow border
    drawGlowBorder(x, y, cellW, cellHeight - 2, paramColor, highlight);

    // Draw label with parameter color
    Parameter param = static_cast<Parameter>(index);
    const char* name = getParameterName(param);

    M5.Display.setTextSize(2);
    M5.Display.setTextColor(paramColor, COLOR_BG);
    M5.Display.setCursor(x + 6, y + 4);
    M5.Display.printf("%d:%-8s", index, name);

    // Draw value in white
    M5.Display.setTextColor(TFT_WHITE, COLOR_BG);
    M5.Display.setCursor(x + cellW - 50, y + 4);
    M5.Display.printf("%3d", value);

    // Draw progress bar
    int barX = x + 6;
    int barY = y + barYOffset;
    int barW = cellW - 12;
    int fillWidth = (static_cast<int32_t>(value) * barW) / 255;

    // Bar background (darker)
    M5.Display.fillRect(barX, barY, barW, barHeight, dimColor(paramColor, 0.15f));

    // Filled portion
    if (fillWidth > 0) {
        M5.Display.fillRect(barX, barY, fillWidth, barHeight, dimColor(paramColor, 0.7f));

        // Bright leading edge (glow effect)
        if (fillWidth > 1) {
            M5.Display.drawFastVLine(barX + fillWidth - 1, barY, barHeight, paramColor);
        }
    }
}

/**
 * Update display with K1 patterns:
 * - State caching (only redraw changed cells)
 * - Per-parameter colors
 * - Glow borders
 * - Progress bars
 */
void updateDisplay(int8_t highlightIdx = -1) {
    if (!g_encoders) return;

    // Update highlight if changed
    if (highlightIdx != s_highlightIndex) {
        int8_t oldHighlight = s_highlightIndex;
        s_highlightIndex = highlightIdx;

        // Force redraw of old and new highlight cells
        if (oldHighlight >= 0 && oldHighlight < 16) {
            s_displayValues[oldHighlight] = 0xFFFF;  // Force redraw
        }
        if (highlightIdx >= 0 && highlightIdx < 16) {
            s_displayValues[highlightIdx] = 0xFFFF;  // Force redraw
        }
    }

    // BEGIN TRANSACTION - batch all display operations (K1 flicker-free pattern)
    M5.Display.startWrite();

    // Selective update: only redraw cells where value changed
    for (uint8_t i = 0; i < 16; i++) {
        uint16_t value = g_encoders->getValue(i);

        // Skip unchanged cells (state caching optimization)
        if (value == s_displayValues[i]) {
            continue;
        }

        // Update cache and redraw cell
        s_displayValues[i] = value;
        bool isHighlight = (i == s_highlightIndex);
        drawCell(i, value, isHighlight);
    }

    // END TRANSACTION - commit all at once (eliminates flicker)
    M5.Display.endWrite();
}

/**
 * Force full display refresh (used after setup/errors)
 */
void forceDisplayRefresh() {
    // Invalidate all cached values
    for (uint8_t i = 0; i < 16; i++) {
        s_displayValues[i] = 0xFFFF;
    }
    updateDisplay();
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

        // Show initial values with K1-style display
        forceDisplayRefresh();

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

        // Show initial values with K1-style display
        forceDisplayRefresh();

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

    // Update encoder service (polls all 16 encoders, handles debounce, fires callbacks)
    // The callback (onEncoderChange) handles display updates with highlighting
    g_encoders->update();

    // Clear highlight after brief delay (fade effect)
    // The callback sets highlight; this clears it after ~500ms of no changes
    static uint32_t lastChange = 0;
    static int8_t lastHighlight = -1;

    if (s_highlightIndex >= 0) {
        lastChange = millis();
        lastHighlight = s_highlightIndex;
    } else if (lastHighlight >= 0 && millis() - lastChange > 500) {
        // Clear highlight and force redraw of that cell
        updateDisplay(-1);
        lastHighlight = -1;
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
