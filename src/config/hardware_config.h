#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include "features.h"

// Hardware pin definitions for LightwaveOS
namespace HardwareConfig {

    // ==================== LED STRIPS CONFIGURATION ====================
    // WS2812 Dual-Strip Configuration for Light Guide Plate
    // Two independent WS2812 strips: GPIO4 (Strip 1), GPIO5 (Strip 2)
    // Each strip has 160 LEDs = 320 total LEDs

    // Strip Configuration
    constexpr uint16_t LEDS_PER_STRIP = 160;  // 160 LEDs per strip
    constexpr uint16_t STRIP1_LED_COUNT = LEDS_PER_STRIP;
    constexpr uint16_t STRIP2_LED_COUNT = LEDS_PER_STRIP;
    constexpr uint16_t TOTAL_LEDS = STRIP1_LED_COUNT + STRIP2_LED_COUNT;  // 320 total
    constexpr uint8_t NUM_STRIPS = 2;

    // GPIO Pin Assignment - WS2812 (single data wire per strip, no clock)
    constexpr uint8_t STRIP1_DATA_PIN = 4;    // WS2812 Strip 1 data - GPIO 4
    constexpr uint8_t STRIP2_DATA_PIN = 5;    // WS2812 Strip 2 data - GPIO 5
    constexpr uint8_t LED_DATA_PIN = STRIP1_DATA_PIN;  // Backward compatibility

    // WS2812 Timing (reference only - handled by FastLED)
    // 800kHz data rate, ~30us per LED, ~9.6ms for 320 LEDs

    // Physical Layout Constants
    constexpr uint16_t STRIP_LENGTH = LEDS_PER_STRIP;   // 160 LEDs per strip
    constexpr uint8_t STRIP_CENTER_POINT = 79;  // LED 79/80 split for outward propagation
    constexpr uint8_t STRIP_HALF_LENGTH = 80;   // 0-79 and 80-159
    
    // Propagation Modes
    enum PropagationMode : uint8_t {
        PROPAGATE_OUTWARD = 0,      // Center (79/80) → Edges (0/159)
        PROPAGATE_INWARD = 1,       // Edges (0/159) → Center (79/80)
        PROPAGATE_LEFT_TO_RIGHT = 2, // 0 → 159 linear
        PROPAGATE_RIGHT_TO_LEFT = 3, // 159 → 0 linear
        PROPAGATE_ALTERNATING = 4    // Back and forth
    };
    
    // Strip Synchronization Modes
    enum SyncMode : uint8_t {
        SYNC_INDEPENDENT = 0,       // Each strip runs different effects
        SYNC_SYNCHRONIZED = 1,      // Both strips show same effect
        SYNC_MIRRORED = 2,          // Strip 2 mirrors Strip 1
        SYNC_CHASE = 3              // Effects bounce between strips
    };
    
    // Strip Performance Settings
    constexpr uint16_t STRIP_FPS = 120;
    constexpr uint8_t STRIP_BRIGHTNESS = 96;   // Default brightness level
    constexpr uint8_t STRIP_MAX_BRIGHTNESS = 160;  // Current limiting for 320 LEDs
    constexpr uint32_t BUTTON_DEBOUNCE_MS = 500;

    // Segment Configuration
    constexpr uint8_t STRIP_SEGMENT_COUNT = 8;     // Divide each strip into 8 segments
    constexpr uint8_t SEGMENT_SIZE = LEDS_PER_STRIP / STRIP_SEGMENT_COUNT;  // 20 LEDs per segment

    // Zone Composer Configuration
    constexpr uint8_t MAX_ZONES = 4;               // Maximum zones per strip
    constexpr uint8_t ZONE_SIZE = 40;              // LEDs per zone (2x 20-LED segments)
    constexpr uint8_t ZONE_SEGMENT_SIZE = 20;      // LEDs per zone segment (left or right)

    // Legacy compatibility
    constexpr uint16_t NUM_LEDS = TOTAL_LEDS;
    constexpr uint16_t DEFAULT_FPS = STRIP_FPS;
    constexpr uint8_t DEFAULT_BRIGHTNESS = STRIP_BRIGHTNESS;

    // Common pins
    constexpr uint8_t POWER_PIN = 48;  // RGB LED power on some DevKits (or use any free GPIO)

    // I2C Configuration for M5Stack 8Encoder
    constexpr uint8_t BUTTON_PIN = 0;       // No button on board
    constexpr uint8_t I2C_SDA = 17;         // I2C SDA on GPIO 17
    constexpr uint8_t I2C_SCL = 18;         // I2C SCL on GPIO 18
    constexpr uint8_t I2C_SDA_SCROLL = 0;
    constexpr uint8_t I2C_SCL_SCROLL = 0;
    constexpr uint8_t M5STACK_8ENCODER_ADDR = 0x41;
    constexpr uint8_t M5UNIT_SCROLL_ADDR = 0x40;
    
    // Memory limits
    constexpr size_t MAX_EFFECTS = 80;  // Increased to accommodate all effects including audio-reactive
    constexpr size_t TRANSITION_BUFFER_SIZE = NUM_LEDS * 3; // RGB bytes
    
    // Light Guide Plate Configuration
    constexpr bool LIGHT_GUIDE_MODE_ENABLED = true;  // Enable LGP-specific features
    constexpr uint8_t LIGHT_GUIDE_MODE_PIN = 255;    // GPIO pin for hardware detection (255 = always enabled)
    constexpr uint32_t LIGHT_GUIDE_SIGNATURE = 0x4C475000;  // "LGP\0" signature for auto-detection
}

// Global I2C mutex for thread-safe Wire operations (stub for compilation when HMI disabled)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
extern SemaphoreHandle_t i2cMutex;

#endif // HARDWARE_CONFIG_H
