#pragma once
// ============================================================================
// PaletteLedDisplay - Palette Color Visualization on Unit B LEDs
// ============================================================================
// Displays 9 representative colors from the current palette on Unit B LEDs 0-8.
// Colors are sorted by hue for visual cohesion (rainbow-like progression).
// All 9 LEDs on Unit B are used for palette display (status LED removed from ENC-B).
//
// Usage:
//   PaletteLedDisplay display(&encoders);
//   display.begin();
//   display.update(paletteId);  // Call when palette changes
//   display.updateAnimation();  // Call in main loop for animations
// ============================================================================

#include <Arduino.h>
#include <math.h>
#include "../config/PaletteLedData.h"

// Forward declaration
class DualEncoderService;

// Animation modes
enum class AnimationMode : uint8_t {
    STATIC = 0,     // No animation - static colors
    ROTATE = 1,     // Colors shift through palette gradient
    WAVE = 2,       // Traveling wave brightness modulation
    BREATHING = 3,  // All LEDs fade in/out together
    SCROLL = 4,     // Colors chase around ring
    MODE_COUNT      // Keep last for count (5 modes total)
};

class PaletteLedDisplay {
public:
    /**
     * Constructor
     * @param encoders Pointer to DualEncoderService (for Unit B LED control)
     */
    explicit PaletteLedDisplay(DualEncoderService* encoders);

    /**
     * Default constructor (encoders set later via setEncoders)
     */
    PaletteLedDisplay();

    /**
     * Set encoder service reference
     * @param encoders Pointer to DualEncoderService
     */
    void setEncoders(DualEncoderService* encoders);

    /**
     * Initialize palette LED display
     * Clears LEDs 0-8 on Unit B and disables rendering
     * Rendering remains disabled until setEnabled(true) is called
     */
    void begin();

    /**
     * Update palette display with new palette ID
     * @param paletteId Palette ID (0-74)
     * @return true if update was successful, false if palette ID invalid or Unit B unavailable
     */
    bool update(uint8_t paletteId);

    /**
     * Clear all palette LEDs (LEDs 0-8 on Unit B)
     */
    void clear();

    /**
     * Enable or disable LED rendering
     * When disabled, LEDs remain off and no rendering occurs
     * @param enabled true to enable rendering, false to disable
     */
    void setEnabled(bool enabled);

    /**
     * Check if LED rendering is enabled
     * @return true if enabled, false if disabled
     */
    bool isEnabled() const { return m_enabled; }

    /**
     * Set brightness scaling factor
     * @param brightness Brightness factor (0-255, where 255 = full brightness)
     */
    void setBrightness(uint8_t brightness);

    /**
     * Get current palette ID being displayed
     * @return Current palette ID, or 255 if none displayed
     */
    uint8_t getCurrentPaletteId() const { return m_currentPaletteId; }

    /**
     * Set animation mode
     * @param mode Animation mode to use
     */
    void setAnimationMode(AnimationMode mode);

    /**
     * Get current animation mode
     * @return Current animation mode
     */
    AnimationMode getAnimationMode() const { return m_animationMode; }

    /**
     * Get animation mode as string
     * @return String representation of current mode
     */
    const char* getAnimationModeString() const;

    /**
     * Update animation (call from main loop)
     * Applies animation effects based on current mode
     */
    void updateAnimation();

    /**
     * Check if Unit B is available
     * @return true if Unit B encoder is available
     */
    bool isAvailable() const;

private:
    DualEncoderService* m_encoders;
    uint8_t m_currentPaletteId;  // 255 = no palette displayed
    uint8_t m_brightness;         // Brightness scaling (0-255)
    bool m_enabled;               // Enable flag - prevents rendering until dashboard loads
    
    // Animation state
    AnimationMode m_animationMode;
    uint32_t m_animationStartTime;
    static constexpr uint32_t ANIMATION_SPEED_MS = 2000;  // 2 seconds per cycle for rotate/scroll
    static constexpr uint32_t BREATHING_PERIOD_MS = 1500;  // 1.5 seconds for breathing
    static constexpr uint8_t BREATHING_MIN_PERCENT = 30;   // 30% min brightness
    static constexpr uint8_t BREATHING_MAX_PERCENT = 100;  // 100% max brightness
    
    // Throttling to prevent excessive LED writes
    uint32_t m_lastAnimationUpdate;  // Last time animation was updated
    static constexpr uint32_t ANIMATION_UPDATE_INTERVAL_MS = 33;  // ~30 FPS max (33ms = 30fps)
    
    // Sample positions for palette (9 evenly-spaced colors)
    static constexpr uint8_t SAMPLE_POSITIONS[9] = {0, 32, 64, 96, 128, 160, 192, 224, 255};

    /**
     * Apply brightness scaling to RGB color
     * @param r Red component (input/output)
     * @param g Green component (input/output)
     * @param b Blue component (input/output)
     */
    void applyBrightness(uint8_t& r, uint8_t& g, uint8_t& b) const;
    
    /**
     * Render static colors (no animation)
     */
    void renderStatic();
    
    /**
     * Render rotate animation
     */
    void renderRotate();
    
    /**
     * Render wave animation
     */
    void renderWave();
    
    /**
     * Render breathing animation
     */
    void renderBreathing();
    
    /**
     * Render scroll/chase animation
     */
    void renderScroll();
};

