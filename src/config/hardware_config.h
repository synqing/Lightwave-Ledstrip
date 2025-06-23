#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include "features.h"

// Hardware pin definitions for Light Crystals
namespace HardwareConfig {

#if LED_STRIPS_MODE
    // ==================== LED STRIPS MODE ====================
    // Dual 160-LED strips in opposite physical layout
    
    // Strip Configuration
    constexpr uint16_t STRIP1_LED_COUNT = 160;
    constexpr uint16_t STRIP2_LED_COUNT = 160;
    constexpr uint16_t TOTAL_LEDS = STRIP1_LED_COUNT + STRIP2_LED_COUNT;  // 320
    constexpr uint8_t NUM_STRIPS = 2;
    
    // GPIO Pin Assignment
    constexpr uint8_t STRIP1_DATA_PIN = 6;   // Channel 1 (primary)
    constexpr uint8_t STRIP2_DATA_PIN = 7;   // Channel 2 (opposite)
    constexpr uint8_t LED_DATA_PIN = STRIP1_DATA_PIN;  // Backward compatibility
    
    // Physical Layout Constants
    constexpr uint8_t STRIP_LENGTH = 160;
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
    constexpr uint8_t STRIP_BRIGHTNESS = 96;
    constexpr uint8_t STRIP_MAX_BRIGHTNESS = 128;  // Current limiting for 320 LEDs
    constexpr uint32_t BUTTON_DEBOUNCE_MS = 500;
    
    // Segment Configuration
    constexpr uint8_t STRIP_SEGMENT_COUNT = 4;     // Divide each strip into 4 segments
    constexpr uint8_t SEGMENT_SIZE = STRIP_LENGTH / STRIP_SEGMENT_COUNT;  // 40 LEDs per segment
    
    // Legacy compatibility
    constexpr uint16_t NUM_LEDS = TOTAL_LEDS;
    constexpr uint16_t DEFAULT_FPS = STRIP_FPS;
    constexpr uint8_t DEFAULT_BRIGHTNESS = STRIP_BRIGHTNESS;
    
#else
    // ==================== LED MATRIX MODE (Original) ====================
    // 9x9 matrix configuration (81 LEDs)
    
    constexpr uint8_t LED_DATA_PIN = 6;     // GPIO6 for matrix
    constexpr uint16_t NUM_LEDS = 81;       // 9x9 matrix
    constexpr uint8_t NUM_STRIPS = 1;       // Single matrix
    constexpr uint16_t DEFAULT_FPS = 120;
    constexpr uint8_t DEFAULT_BRIGHTNESS = 96;
    constexpr uint32_t BUTTON_DEBOUNCE_MS = 500;
    
#endif

    // Common pins for both modes
    constexpr uint8_t BUTTON_PIN = 0;
    constexpr uint8_t POWER_PIN = 5;
    
    // Audio input pins (for future use)
    constexpr uint8_t I2S_SCK = 3;
    constexpr uint8_t I2S_WS = 2;
    constexpr uint8_t I2S_DIN = 4;
    
    // Memory limits
    constexpr size_t MAX_EFFECTS = 20;
    constexpr size_t TRANSITION_BUFFER_SIZE = NUM_LEDS * 3; // RGB bytes
}

#endif // HARDWARE_CONFIG_H