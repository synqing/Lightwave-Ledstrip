// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// PaletteLedDisplay - Palette Color Visualization on Unit B LEDs
// ============================================================================
// Implementation: Displays 8 representative colors from current palette
// on Unit B LEDs 0-8. All 9 LEDs are used for palette display.
// ============================================================================

#include "PaletteLedDisplay.h"
#include "../input/DualEncoderService.h"
#include "../input/Rotate8Transport.h"

// ============================================================================
// Constructors
// ============================================================================

PaletteLedDisplay::PaletteLedDisplay()
    : m_encoders(nullptr)
    , m_currentPaletteId(255)  // 255 = no palette displayed
    , m_brightness(255)         // Full brightness by default
    , m_animationMode(AnimationMode::ROTATE)  // Default to rotate animation
    , m_animationStartTime(0)
    , m_enabled(false)          // Disabled by default until dashboard loads
{
}

PaletteLedDisplay::PaletteLedDisplay(DualEncoderService* encoders)
    : m_encoders(encoders)
    , m_currentPaletteId(255)
    , m_brightness(255)
    , m_animationMode(AnimationMode::ROTATE)  // Default to rotate animation
    , m_animationStartTime(0)
    , m_enabled(false)          // Disabled by default until dashboard loads
{
}

// ============================================================================
// Initialization
// ============================================================================

void PaletteLedDisplay::setEncoders(DualEncoderService* encoders) {
    m_encoders = encoders;
}

void PaletteLedDisplay::begin() {
    // Clear palette LEDs on initialization (but keep disabled)
    // LEDs will remain off until setEnabled(true) is called
    m_enabled = false;
    clear();
    m_animationMode = AnimationMode::STATIC;
    m_animationStartTime = millis();
    m_lastAnimationUpdate = 0;  // Initialize throttling
}

// ============================================================================
// Update Display
// ============================================================================

bool PaletteLedDisplay::update(uint8_t paletteId) {
    // #region agent log (DISABLED)
    // uint32_t now = millis();
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A,B,E\",\"location\":\"PaletteLedDisplay.cpp:51\",\"message\":\"update.entry\",\"data\":{\"paletteId\":%u,\"currentPaletteId\":%u,\"mode\":%u,\"available\":%d,\"timestamp\":%lu}\n",
        // paletteId, m_currentPaletteId, static_cast<uint8_t>(m_animationMode),
        // isAvailable() ? 1 : 0, static_cast<unsigned long>(now));
        // #endregion
    
    // Validate palette ID
    if (paletteId >= PALETTE_LED_COUNT) {
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"PaletteLedDisplay.cpp:59\",\"message\":\"update.invalidPalette\",\"data\":{\"paletteId\":%u,\"max\":%u,\"timestamp\":%lu}\n",
            // paletteId, PALETTE_LED_COUNT, static_cast<unsigned long>(millis()));
                // #endregion
        return false;
    }

    // Check if Unit B is available
    if (!isAvailable()) {
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"PaletteLedDisplay.cpp:65\",\"message\":\"update.unavailable\",\"data\":{\"timestamp\":%lu}\n",
            // static_cast<unsigned long>(millis()));
                // #endregion
        return false;
    }

    // Update palette ID (even if same, animation may need refresh)
    bool paletteChanged = (m_currentPaletteId != paletteId);
    m_currentPaletteId = paletteId;
    
    // Reset animation start time when palette changes
    m_animationStartTime = millis();

    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A,B\",\"location\":\"PaletteLedDisplay.cpp:75\",\"message\":\"update.beforeRender\",\"data\":{\"paletteChanged\":%d,\"mode\":%u,\"willRenderStatic\":%d,\"timestamp\":%lu}\n",
        // paletteChanged ? 1 : 0, static_cast<uint8_t>(m_animationMode),
        // (m_animationMode == AnimationMode::STATIC) ? 1 : 0, static_cast<unsigned long>(millis()));
        // #endregion

    // CRITICAL FIX: Always render immediately when palette changes, regardless of mode
    // This ensures palette changes "latch" immediately instead of waiting for next animation cycle
    if (paletteChanged) {
        switch (m_animationMode) {
            case AnimationMode::STATIC:
                renderStatic();
                break;
            case AnimationMode::ROTATE:
                renderRotate();
                break;
            case AnimationMode::WAVE:
                renderWave();
                break;
            case AnimationMode::BREATHING:
                renderBreathing();
                break;
            case AnimationMode::SCROLL:
                renderScroll();
                break;
            default:
                renderStatic();
                break;
        }
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"PaletteLedDisplay.cpp:100\",\"message\":\"update.renderedImmediate\",\"data\":{\"mode\":%u,\"timestamp\":%lu}\n",
            // static_cast<uint8_t>(m_animationMode), static_cast<unsigned long>(millis()));
                // #endregion
    } else if (m_animationMode == AnimationMode::STATIC) {
        // Only render static if palette didn't change (refresh static display)
        renderStatic();
    }
    // Otherwise, animation will be handled by updateAnimation() (but only when needed)

    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"PaletteLedDisplay.cpp:84\",\"message\":\"update.exit\",\"data\":{\"timestamp\":%lu}\n",
        // static_cast<unsigned long>(millis()));
        // #endregion

    return true;
}

// ============================================================================
// Clear Display
// ============================================================================

void PaletteLedDisplay::clear() {
    if (!isAvailable()) {
        return;
    }

    // Get Unit B transport
    Rotate8Transport& transportB = m_encoders->transportB();

    // Turn off LEDs 0-8
    for (uint8_t ledIndex = 0; ledIndex < PALETTE_COLORS_PER_PALETTE; ledIndex++) {
        transportB.setLED(ledIndex, 0, 0, 0);
    }

    // Reset cached palette ID
    m_currentPaletteId = 255;
}

// ============================================================================
// Enable/Disable Control
// ============================================================================

void PaletteLedDisplay::setEnabled(bool enabled) {
    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"palette-latch\",\"hypothesisId\":\"B\",\"location\":\"PaletteLedDisplay.cpp:setEnabled\",\"message\":\"enable.change\",\"data\":{\"wasEnabled\":%d,\"nowEnabled\":%d,\"timestamp\":%lu}}\n",
        // m_enabled ? 1 : 0, enabled ? 1 : 0, static_cast<unsigned long>(millis()));
        // #endregion
    m_enabled = enabled;
    if (!enabled) {
        // When disabled, turn off all LEDs
        clear();
    }
}

// ============================================================================
// Brightness Control
// ============================================================================

void PaletteLedDisplay::setBrightness(uint8_t brightness) {
    m_brightness = brightness;

    // If a palette is currently displayed, refresh with new brightness
    if (m_currentPaletteId < PALETTE_LED_COUNT) {
        update(m_currentPaletteId);
    }
}

void PaletteLedDisplay::applyBrightness(uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (m_brightness == 255) {
        // Full brightness - no scaling needed
        return;
    }

    // Scale each component by brightness factor
    r = (r * m_brightness) / 255;
    g = (g * m_brightness) / 255;
    b = (b * m_brightness) / 255;
}

// ============================================================================
// Availability Check
// ============================================================================

bool PaletteLedDisplay::isAvailable() const {
    if (!m_encoders) {
        return false;
    }
    return m_encoders->isUnitBAvailable();
}

// ============================================================================
// Animation Control
// ============================================================================

void PaletteLedDisplay::setAnimationMode(AnimationMode mode) {
    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"PaletteLedDisplay.cpp:130\",\"message\":\"setAnimationMode\",\"data\":{\"oldMode\":%u,\"newMode\":%u,\"startTimeReset\":%lu},\"timestamp\":%lu}\n",
        // static_cast<uint8_t>(m_animationMode), static_cast<uint8_t>(mode),
        // static_cast<unsigned long>(millis()), static_cast<unsigned long>(millis()));
        // #endregion
    
    if (mode == m_animationMode) {
        return;  // No change
    }
    
    m_animationMode = mode;
    m_animationStartTime = millis();  // Reset animation timing
    
    // If switching to static, render immediately
    if (mode == AnimationMode::STATIC) {
        renderStatic();
    }
}

const char* PaletteLedDisplay::getAnimationModeString() const {
    switch (m_animationMode) {
        case AnimationMode::STATIC:    return "static";
        case AnimationMode::ROTATE:    return "rotate";
        case AnimationMode::WAVE:      return "wave";
        case AnimationMode::BREATHING: return "breathing";
        case AnimationMode::SCROLL:    return "scroll";
        default:                        return "unknown";
    }
}

void PaletteLedDisplay::updateAnimation() {
    // Early exit if disabled - prevents rendering until dashboard loads
    if (!m_enabled) {
        return;
    }

    uint32_t now = millis();  // Needed for throttling logic below

    // #region agent log (DISABLED)
    // (now variable moved outside debug region)
    // static uint32_t s_lastLogTime = 0;
    // static uint32_t s_callCount = 0;
    // s_callCount++;
    // if (now - s_lastLogTime >= 100) {  // Log every 100ms
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A,C,E\",\"location\":\"PaletteLedDisplay.cpp:200\",\"message\":\"updateAnimation.entry\",\"data\":{\"available\":%d,\"paletteId\":%u,\"mode\":%u,\"callsSinceLastLog\":%lu,\"timestamp\":%lu}\n",
            // isAvailable() ? 1 : 0, m_currentPaletteId, static_cast<uint8_t>(m_animationMode),
            // static_cast<unsigned long>(s_callCount), static_cast<unsigned long>(now));
        // s_callCount = 0;
        // s_lastLogTime = now;
    // }
        // #endregion
    
    if (!isAvailable() || m_currentPaletteId >= PALETTE_LED_COUNT) {
        // #region agent log (DISABLED)
        // static uint32_t s_lastSkipLog = 0;
        // if (now - s_lastSkipLog >= 500) {
            // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"E\",\"location\":\"PaletteLedDisplay.cpp:214\",\"message\":\"updateAnimation.skipped\",\"data\":{\"available\":%d,\"paletteId\":%u,\"timestamp\":%lu}\n",
                // isAvailable() ? 1 : 0, m_currentPaletteId, static_cast<unsigned long>(now));
            // s_lastSkipLog = now;
        // }
                // #endregion
        return;  // No palette to animate
    }
    
    // Only animate if not in static mode
    if (m_animationMode == AnimationMode::STATIC) {
        // #region agent log (DISABLED)
        // static uint32_t s_lastStaticSkipLog = 0;
        // if (now - s_lastStaticSkipLog >= 1000) {
            // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"B\",\"location\":\"PaletteLedDisplay.cpp:225\",\"message\":\"updateAnimation.skippedStatic\",\"data\":{\"timestamp\":%lu}\n",
                // static_cast<unsigned long>(now));
            // s_lastStaticSkipLog = now;
        // }
                // #endregion
        return;
    }
    
    // CRITICAL FIX: Throttle animation updates to prevent excessive LED writes
    // Only update if enough time has passed since last update (rate limit to ~30 FPS)
    if (now - m_lastAnimationUpdate < ANIMATION_UPDATE_INTERVAL_MS) {
        // #region agent log (DISABLED)
        // static uint32_t s_lastThrottleLog = 0;
        // if (now - s_lastThrottleLog >= 1000) {
            // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"PaletteLedDisplay.cpp:235\",\"message\":\"updateAnimation.throttled\",\"data\":{\"timeSinceLastUpdate\":%lu,\"timestamp\":%lu}\n",
                // static_cast<unsigned long>(now - m_lastAnimationUpdate), static_cast<unsigned long>(now));
            // s_lastThrottleLog = now;
        // }
                // #endregion
        return;  // Too soon, skip this update
    }
    
    m_lastAnimationUpdate = now;
    
    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A,C\",\"location\":\"PaletteLedDisplay.cpp:245\",\"message\":\"updateAnimation.rendering\",\"data\":{\"mode\":%u,\"timestamp\":%lu}\n",
        // static_cast<uint8_t>(m_animationMode), static_cast<unsigned long>(now));
        // #endregion
    
    // Route to appropriate animation renderer
    switch (m_animationMode) {
        case AnimationMode::ROTATE:
            renderRotate();
            break;
        case AnimationMode::WAVE:
            renderWave();
            break;
        case AnimationMode::BREATHING:
            renderBreathing();
            break;
        case AnimationMode::SCROLL:
            renderScroll();
            break;
        default:
            renderStatic();
            break;
    }
    
    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A,C\",\"location\":\"PaletteLedDisplay.cpp:265\",\"message\":\"updateAnimation.exit\",\"data\":{\"timestamp\":%lu}\n",
        // static_cast<unsigned long>(millis()));
        // #endregion
}

// ============================================================================
// Animation Renderers
// ============================================================================

void PaletteLedDisplay::renderStatic() {
    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"palette-latch\",\"hypothesisId\":\"B,E\",\"location\":\"PaletteLedDisplay.cpp:renderStatic\",\"message\":\"render.entry\",\"data\":{\"enabled\":%d,\"available\":%d,\"paletteId\":%u,\"timestamp\":%lu}}\n",
        // m_enabled ? 1 : 0, isAvailable() ? 1 : 0, m_currentPaletteId, static_cast<unsigned long>(millis()));
        // #endregion
    
    // Early exit if disabled or unavailable
    if (!m_enabled || !isAvailable() || m_currentPaletteId >= PALETTE_LED_COUNT) {
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"palette-latch\",\"hypothesisId\":\"B,E\",\"location\":\"PaletteLedDisplay.cpp:renderStatic\",\"message\":\"render.early_exit\",\"data\":{\"reason\":\"%s\",\"timestamp\":%lu}}\n",
            // !m_enabled ? "disabled" : (!isAvailable() ? "unavailable" : "invalid_palette"), static_cast<unsigned long>(millis()));
                // #endregion
        return;
    }
    
    Rotate8Transport& transportB = m_encoders->transportB();
    
    // Update each LED (0-8) with palette colors
    for (uint8_t ledIndex = 0; ledIndex < PALETTE_COLORS_PER_PALETTE; ledIndex++) {
        uint8_t r, g, b;
        getPaletteColor(m_currentPaletteId, ledIndex, r, g, b);
        
        // Apply brightness scaling
        applyBrightness(r, g, b);
        
        // #region agent log (DISABLED)
        // if (ledIndex == 0 || ledIndex == 8) {  // Log first and last LED
            // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"palette-latch\",\"hypothesisId\":\"F\",\"location\":\"PaletteLedDisplay.cpp:renderStatic\",\"message\":\"led.write\",\"data\":{\"led\":%u,\"r\":%u,\"g\":%u,\"b\":%u,\"timestamp\":%lu}}\n",
                // ledIndex, r, g, b, static_cast<unsigned long>(millis()));
        // }
                // #endregion
        
        // Set LED on Unit B (LEDs 0-8)
        transportB.setLED(ledIndex, r, g, b);
    }
    
    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"palette-latch\",\"hypothesisId\":\"F\",\"location\":\"PaletteLedDisplay.cpp:renderStatic\",\"message\":\"render.complete\",\"data\":{\"timestamp\":%lu}}\n",
        // static_cast<unsigned long>(millis()));
        // #endregion
}

void PaletteLedDisplay::renderRotate() {
    // Early exit if disabled or unavailable
    if (!m_enabled || !isAvailable() || m_currentPaletteId >= PALETTE_LED_COUNT) {
        return;
    }
    
    Rotate8Transport& transportB = m_encoders->transportB();
    uint32_t elapsed = millis() - m_animationStartTime;
    
    // Calculate time offset (0-255) that cycles through palette
    uint8_t timeOffset = ((elapsed / ANIMATION_SPEED_MS) * 256) % 256;
    
    // #region agent log (DISABLED)
    // static uint32_t s_lastRotateLog = 0;
    // if (millis() - s_lastRotateLog >= 500) {  // Log every 500ms
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H4\",\"location\":\"PaletteLedDisplay.cpp:217\",\"message\":\"renderRotate.timing\",\"data\":{\"elapsed\":%lu,\"timeOffset\":%u,\"speedMs\":%lu},\"timestamp\":%lu}\n",
            // static_cast<unsigned long>(elapsed), timeOffset, static_cast<unsigned long>(ANIMATION_SPEED_MS),
            // static_cast<unsigned long>(millis()));
        // s_lastRotateLog = millis();
    // }
        // #endregion
    
    // Update each LED with rotated palette position
    for (uint8_t ledIndex = 0; ledIndex < PALETTE_COLORS_PER_PALETTE; ledIndex++) {
        // Calculate palette position with rotation offset
        uint16_t basePos = SAMPLE_POSITIONS[ledIndex];
        uint16_t palettePos = (basePos + timeOffset) % 256;
        
        // Interpolate color from palette at this position
        // We need to find which sample positions bracket this palettePos
        uint8_t r, g, b;
        
        // Find the two sample positions that bracket palettePos
        uint8_t lowerIdx = 0;
        uint8_t upperIdx = 0;
        
        for (uint8_t i = 0; i < (PALETTE_COLORS_PER_PALETTE - 1); i++) {
            if (SAMPLE_POSITIONS[i] <= palettePos && palettePos <= SAMPLE_POSITIONS[i + 1]) {
                lowerIdx = i;
                upperIdx = i + 1;
                break;
            }
        }
        
        // Handle wrap-around case (palettePos > 255 or < 0)
        if (palettePos >= SAMPLE_POSITIONS[7]) {
            // Use last sample position
            getPaletteColor(m_currentPaletteId, 7, r, g, b);
        } else if (palettePos <= SAMPLE_POSITIONS[0]) {
            // Use first sample position
            getPaletteColor(m_currentPaletteId, 0, r, g, b);
        } else {
            // Interpolate between two sample positions
            uint16_t lowerPos = SAMPLE_POSITIONS[lowerIdx];
            uint16_t upperPos = SAMPLE_POSITIONS[upperIdx];
            uint16_t range = upperPos - lowerPos;
            
            if (range == 0) {
                getPaletteColor(m_currentPaletteId, lowerIdx, r, g, b);
            } else {
                uint8_t r1, g1, b1, r2, g2, b2;
                getPaletteColor(m_currentPaletteId, lowerIdx, r1, g1, b1);
                getPaletteColor(m_currentPaletteId, upperIdx, r2, g2, b2);
                
                uint16_t ratio = ((palettePos - lowerPos) * 255) / range;
                r = r1 + ((r2 - r1) * ratio) / 255;
                g = g1 + ((g2 - g1) * ratio) / 255;
                b = b1 + ((b2 - b1) * ratio) / 255;
            }
        }
        
        // Apply brightness scaling
        applyBrightness(r, g, b);
        
        // #region agent log (DISABLED)
        // if (ledIndex == 0) {  // Log first LED only to avoid spam
            // static uint32_t s_lastLedLog = 0;
            // if (millis() - s_lastLedLog >= 500) {
                // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2,H4\",\"location\":\"PaletteLedDisplay.cpp:281\",\"message\":\"renderRotate.ledUpdate\",\"data\":{\"ledIndex\":%u,\"basePos\":%u,\"palettePos\":%u,\"r\":%u,\"g\":%u,\"b\":%u},\"timestamp\":%lu}\n",
                    // ledIndex, basePos, palettePos, r, g, b, static_cast<unsigned long>(millis()));
                // s_lastLedLog = millis();
            // }
        // }
                // #endregion
        
        
        // Set LED on Unit B
        transportB.setLED(ledIndex, r, g, b);
    }
}

void PaletteLedDisplay::renderWave() {
    // Early exit if disabled or unavailable
    if (!m_enabled || !isAvailable() || m_currentPaletteId >= PALETTE_LED_COUNT) {
        return;
    }
    
    Rotate8Transport& transportB = m_encoders->transportB();
    uint32_t elapsed = millis() - m_animationStartTime;
    
    // Calculate base phase (1Hz = 1000ms period)
    float phase = (elapsed / 1000.0f) * 2.0f * M_PI;
    
    // Update each LED with wave brightness modulation
    for (uint8_t ledIndex = 0; ledIndex < PALETTE_COLORS_PER_PALETTE; ledIndex++) {
        // Phase offset per LED (45 degrees = π/4 per LED)
        float ledPhase = phase + (ledIndex * M_PI / 4.0f);
        
        // Calculate wave value (0-1)
        float wave = (sinf(ledPhase) + 1.0f) / 2.0f;
        
        // Get base palette color
        uint8_t r, g, b;
        getPaletteColor(m_currentPaletteId, ledIndex, r, g, b);
        
        // Apply wave brightness modulation (50-100% range)
        uint8_t waveBrightness = 128 + (wave * 127);  // 128-255 (50-100%)
        r = (r * waveBrightness) / 255;
        g = (g * waveBrightness) / 255;
        b = (b * waveBrightness) / 255;
        
        // Apply global brightness scaling
        applyBrightness(r, g, b);
        
        
        // Set LED on Unit B
        transportB.setLED(ledIndex, r, g, b);
    }
}

void PaletteLedDisplay::renderBreathing() {
    // Early exit if disabled or unavailable
    if (!m_enabled || !isAvailable() || m_currentPaletteId >= PALETTE_LED_COUNT) {
        return;
    }
    
    Rotate8Transport& transportB = m_encoders->transportB();
    uint32_t elapsed = millis() - m_animationStartTime;
    
    // Calculate phase within breathing cycle (0.0 to 1.0)
    float phase = static_cast<float>(elapsed % BREATHING_PERIOD_MS) / static_cast<float>(BREATHING_PERIOD_MS);
    
    // Use sine wave for smooth breathing
    float sineValue = (sinf(phase * 2.0f * M_PI) + 1.0f) / 2.0f;
    
    // Map to brightness range [MIN_PERCENT, MAX_PERCENT]
    float minFactor = static_cast<float>(BREATHING_MIN_PERCENT) / 100.0f;
    float maxFactor = static_cast<float>(BREATHING_MAX_PERCENT) / 100.0f;
    float brightnessFactor = minFactor + sineValue * (maxFactor - minFactor);
    
    // Apply breathing to all LEDs uniformly
    for (uint8_t ledIndex = 0; ledIndex < PALETTE_COLORS_PER_PALETTE; ledIndex++) {
        uint8_t r, g, b;
        getPaletteColor(m_currentPaletteId, ledIndex, r, g, b);
        
        // Apply breathing brightness
        r = static_cast<uint8_t>(r * brightnessFactor);
        g = static_cast<uint8_t>(g * brightnessFactor);
        b = static_cast<uint8_t>(b * brightnessFactor);
        
        // Apply global brightness scaling
        applyBrightness(r, g, b);
        
        
        // Set LED on Unit B
        transportB.setLED(ledIndex, r, g, b);
    }
}

void PaletteLedDisplay::renderScroll() {
    // Early exit if disabled or unavailable
    if (!m_enabled || !isAvailable() || m_currentPaletteId >= PALETTE_LED_COUNT) {
        return;
    }
    
    Rotate8Transport& transportB = m_encoders->transportB();
    uint32_t elapsed = millis() - m_animationStartTime;
    
    // Calculate scroll offset (0-7) that cycles through LED positions
    uint8_t offset = ((elapsed / ANIMATION_SPEED_MS) * 8) % 8;
    
    // #region agent log (DISABLED)
    // static uint32_t s_lastScrollLog = 0;
    // if (millis() - s_lastScrollLog >= 500) {
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H4\",\"location\":\"PaletteLedDisplay.cpp:410\",\"message\":\"renderScroll.timing\",\"data\":{\"elapsed\":%lu,\"offset\":%u},\"timestamp\":%lu}\n",
            // static_cast<unsigned long>(elapsed), offset, static_cast<unsigned long>(millis()));
        // s_lastScrollLog = millis();
    // }
        // #endregion
    
    // Update each LED with scrolled color assignment
    for (uint8_t ledIndex = 0; ledIndex < PALETTE_COLORS_PER_PALETTE; ledIndex++) {
        // Calculate source LED index (wraps around)
        uint8_t sourceLedIndex = (ledIndex + offset) % 8;
        
        // Get color from source LED index
        uint8_t r, g, b;
        getPaletteColor(m_currentPaletteId, sourceLedIndex, r, g, b);
        
        // Apply brightness scaling
        applyBrightness(r, g, b);
        
        
        // Set LED on Unit B
        transportB.setLED(ledIndex, r, g, b);
    }
}

