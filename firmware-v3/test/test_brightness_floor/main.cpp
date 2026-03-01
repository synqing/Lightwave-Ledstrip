/**
 * @file main.cpp
 * @brief Brightness Floor Perceptual Test
 *
 * Standalone test firmware for empirical validation of gamma-corrected
 * brightness levels on WS2812B LED strips in a dark room.
 *
 * Hardware config extracted from firmware-v3:
 *   - ESP32-S3 N16R8
 *   - Strip 1: GPIO 4, 160 LEDs, WS2812 GRB
 *   - Strip 2: GPIO 5, 160 LEDs, WS2812 GRB
 *   - Power:   5V / 3000mA max
 *   - Color correction: TypicalLEDStrip
 *   - RMT driver: Custom (FASTLED_RMT_BUILTIN_DRIVER=0)
 *
 * Two test phases:
 *
 *   PHASE 1 — Brightness Scaler Method (production-relevant)
 *     Pixels set to full warm white (R=255, G=180, B=100).
 *     FastLED.setBrightness() set to gamma-corrected byte value.
 *     FastLED temporal dithering active — can produce sub-integer
 *     effective brightness via rapid toggling.
 *
 *   PHASE 2 — Direct PWM Method (raw hardware test)
 *     FastLED.setBrightness(255) fixed.
 *     Pixel colors set to per-channel gamma-corrected values.
 *     No dithering benefit — pure 8-bit quantisation visible.
 *
 * Gamma correction formula:
 *   linear_fraction = (perceptual_percent / 100.0) ^ 2.2
 *   byte_value      = round(255 * linear_fraction)       [Phase 1]
 *   channel_value   = round(channel_max * linear_fraction) [Phase 2]
 *
 * Build:  pio run -e test_brightness_floor
 * Flash:  pio run -e test_brightness_floor -t upload
 * Serial: pio device monitor -e test_brightness_floor
 */

#include <Arduino.h>
#include <FastLED.h>
#include <cmath>

// ============================================================================
// Hardware Configuration (from firmware-v3 chip_esp32s3.h + RendererActor.h)
// ============================================================================

static constexpr uint8_t  STRIP1_PIN       = 4;   // chip::gpio::LED_STRIP1_DATA
static constexpr uint8_t  STRIP2_PIN       = 5;   // chip::gpio::LED_STRIP2_DATA
static constexpr uint16_t LEDS_PER_STRIP   = 160;  // LedConfig::LEDS_PER_STRIP
static constexpr uint16_t TOTAL_LEDS       = LEDS_PER_STRIP * 2;  // 320

// LED buffers
CRGB strip1[LEDS_PER_STRIP];
CRGB strip2[LEDS_PER_STRIP];

// ============================================================================
// Test Configuration
// ============================================================================

static constexpr float GAMMA = 2.2f;

// Warm white base color at 100% brightness (pre-scaling)
static constexpr uint8_t BASE_R = 255;
static constexpr uint8_t BASE_G = 180;
static constexpr uint8_t BASE_B = 100;

// Perceptual brightness levels to test (percent)
// Includes the 5 requested levels plus reference anchors
static constexpr float TEST_LEVELS[] = {
    1.0f,   // Below quantisation — expect black (byte 0)
    3.0f,   // Below quantisation — expect black (byte 0)
    5.0f,   // Below quantisation — expect black (byte 0)
    8.0f,   // Theoretical floor — byte 1 (~0.39% duty)
    10.0f,  // Byte 2 (~0.63% duty)
    15.0f,  // Reference anchor — byte 4
    20.0f,  // Reference anchor — byte 7
    30.0f,  // Clearly visible reference — byte 18
};
static constexpr int NUM_LEVELS = sizeof(TEST_LEVELS) / sizeof(TEST_LEVELS[0]);

// Hold time per level in milliseconds
static constexpr uint32_t HOLD_TIME_MS = 5000;

// ============================================================================
// Gamma Correction Utilities
// ============================================================================

/**
 * Apply gamma 2.2 correction to a perceptual brightness percentage.
 * Returns the linear duty-cycle fraction [0.0 .. 1.0].
 */
static float gammaCorrect(float perceptualPercent) {
    return powf(perceptualPercent / 100.0f, GAMMA);
}

/**
 * Convert a linear fraction to a 0-255 byte value.
 */
static uint8_t linearToByte(float linearFraction) {
    float scaled = 255.0f * linearFraction;
    if (scaled < 0.0f) return 0;
    if (scaled > 255.0f) return 255;
    return static_cast<uint8_t>(roundf(scaled));
}

/**
 * Scale a channel value (0-255) by a linear fraction.
 */
static uint8_t scaleChannel(uint8_t channelMax, float linearFraction) {
    float scaled = channelMax * linearFraction;
    if (scaled < 0.0f) return 0;
    if (scaled > 255.0f) return 255;
    return static_cast<uint8_t>(roundf(scaled));
}

// ============================================================================
// Fill Helpers
// ============================================================================

static void fillAll(CRGB color) {
    fill_solid(strip1, LEDS_PER_STRIP, color);
    fill_solid(strip2, LEDS_PER_STRIP, color);
}

/**
 * Hold the current LED state for the specified duration,
 * refreshing at ~60 Hz so FastLED temporal dithering can operate.
 */
static void holdWithDithering(uint32_t durationMs) {
    uint32_t elapsed = 0;
    while (elapsed < durationMs) {
        FastLED.show();
        delay(16);       // ~60 Hz refresh for dithering
        elapsed += 16;
    }
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);  // Let serial + USB CDC settle

    Serial.println();
    Serial.println("================================================================");
    Serial.println("  BRIGHTNESS FLOOR PERCEPTUAL TEST");
    Serial.println("  WS2812B Gamma 2.2 Dark Room Validation");
    Serial.println("================================================================");
    Serial.println();
    Serial.printf("Hardware:   2 x %d LEDs on GPIO %d / %d (WS2812 GRB)\n",
                   LEDS_PER_STRIP, STRIP1_PIN, STRIP2_PIN);
    Serial.printf("Base color: R=%d G=%d B=%d (warm white)\n", BASE_R, BASE_G, BASE_B);
    Serial.printf("Gamma:      %.1f\n", GAMMA);
    Serial.printf("Hold time:  %d seconds per level\n", HOLD_TIME_MS / 1000);
    Serial.printf("Levels:     %d\n", NUM_LEVELS);
    Serial.println();

    // Pre-compute and display the full level table
    Serial.println("PRE-COMPUTED LEVEL TABLE:");
    Serial.println("                              Phase 1 (scaler)   Phase 2 (per-channel)");
    Serial.println("Level  Perceptual  Linear%    Brightness byte     R    G    B");
    Serial.println("-----  ----------  --------   ---------------    ---  ---  ---");
    for (int i = 0; i < NUM_LEVELS; i++) {
        float linear = gammaCorrect(TEST_LEVELS[i]);
        uint8_t bri = linearToByte(linear);
        uint8_t r = scaleChannel(BASE_R, linear);
        uint8_t g = scaleChannel(BASE_G, linear);
        uint8_t b = scaleChannel(BASE_B, linear);
        Serial.printf("  %d      %4.1f%%     %7.4f%%         %3d            %3d  %3d  %3d\n",
                       i + 1, TEST_LEVELS[i], linear * 100.0f, bri, r, g, b);
    }
    Serial.println();

    // Initialize FastLED — dual strip, same config as firmware-v3
    FastLED.addLeds<WS2812, STRIP1_PIN, GRB>(strip1, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, STRIP2_PIN, GRB>(strip2, LEDS_PER_STRIP);

    // Match firmware-v3 settings
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setDither(1);       // Temporal dithering ON (critical for low values)
    FastLED.setMaxRefreshRate(0, true);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 3000);
    FastLED.setBrightness(255);
    FastLED.clear(true);

    Serial.println("FastLED initialized. Dithering: ON");
    Serial.println("================================================================");
    Serial.println();
    Serial.println("TEST BEGINS IN 3 SECONDS...");
    delay(3000);
}

// ============================================================================
// Main Loop — Two-Phase Test Cycle
// ============================================================================

static int currentPhase = 0;  // 0 = Phase 1 (scaler), 1 = Phase 2 (per-channel)
static int currentLevel = -1;

void loop() {
    // Advance level; when all levels done, switch phase
    currentLevel++;
    if (currentLevel >= NUM_LEVELS) {
        currentLevel = 0;
        currentPhase = (currentPhase + 1) % 2;

        // Brief blackout between phases
        fillAll(CRGB::Black);
        FastLED.setBrightness(255);
        FastLED.show();

        Serial.println();
        Serial.println("================================================================");
        if (currentPhase == 0) {
            Serial.println("  PHASE 1: BRIGHTNESS SCALER METHOD");
            Serial.println("  Pixels = full warm white, brightness varied via scaler");
            Serial.println("  FastLED dithering active on brightness scaler");
        } else {
            Serial.println("  PHASE 2: DIRECT PWM METHOD");
            Serial.println("  Brightness scaler = 255 (fixed), pixel values varied");
            Serial.println("  Raw 8-bit quantisation — dithering less effective");
        }
        Serial.println("================================================================");
        Serial.println();
        delay(2000);
    }

    float perceptual = TEST_LEVELS[currentLevel];
    float linear = gammaCorrect(perceptual);

    Serial.println("----------------------------------------");

    if (currentPhase == 0) {
        // Phase 1: Brightness Scaler Method
        uint8_t bri = linearToByte(linear);

        Serial.printf("PHASE 1 | Level %d/%d: %.1f%% perceptual\n",
                       currentLevel + 1, NUM_LEVELS, perceptual);
        Serial.printf("  Linear: %.4f%%  |  Brightness byte: %d/255\n",
                       linear * 100.0f, bri);
        Serial.printf("  Pixels: R=%d G=%d B=%d (full warm white)\n",
                       BASE_R, BASE_G, BASE_B);
        if (bri == 0) {
            Serial.println("  NOTE: Byte rounds to 0 — dithering may produce faint glow");
        }

        fillAll(CRGB(BASE_R, BASE_G, BASE_B));
        FastLED.setBrightness(bri);

    } else {
        // Phase 2: Direct PWM Method
        uint8_t r = scaleChannel(BASE_R, linear);
        uint8_t g = scaleChannel(BASE_G, linear);
        uint8_t b = scaleChannel(BASE_B, linear);

        Serial.printf("PHASE 2 | Level %d/%d: %.1f%% perceptual\n",
                       currentLevel + 1, NUM_LEVELS, perceptual);
        Serial.printf("  Linear: %.4f%%  |  Brightness scaler: 255 (fixed)\n",
                       linear * 100.0f);
        Serial.printf("  Pixels: R=%d G=%d B=%d (gamma-scaled channels)\n", r, g, b);
        if (r == 0 && g == 0 && b == 0) {
            Serial.println("  NOTE: All channels round to 0 — expect black");
        }

        FastLED.setBrightness(255);
        fillAll(CRGB(r, g, b));
    }

    Serial.printf("  Hold: %d seconds\n", HOLD_TIME_MS / 1000);
    Serial.println("----------------------------------------");

    // Hold with dithering refresh
    holdWithDithering(HOLD_TIME_MS);
}
