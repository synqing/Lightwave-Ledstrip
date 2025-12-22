/**
 * @file RendererActor.h
 * @brief Actor responsible for LED rendering at 120 FPS
 *
 * The RendererActor is the heart of the visual system. It:
 * - Runs on Core 1 at highest priority for deterministic timing
 * - Maintains the LED buffer state
 * - Executes effect render functions at 120 FPS
 * - Handles brightness, speed, and palette changes
 * - Publishes FRAME_RENDERED events for synchronization
 *
 * Architecture:
 *   Commands (from other actors/cores):
 *     SET_EFFECT, SET_BRIGHTNESS, SET_SPEED, SET_PALETTE, etc.
 *
 *   Events (published to MessageBus):
 *     FRAME_RENDERED - After each successful render
 *     EFFECT_CHANGED - When effect changes
 *
 * Thread Safety:
 *   The RendererActor owns the LED buffer exclusively.
 *   Other actors must NOT directly access leds[] or call FastLED.
 *   Use messages to request state changes.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "Actor.h"
#include "../bus/MessageBus.h"
#include "../../effects/enhancement/ColorCorrectionEngine.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

// Forward declarations
namespace lightwaveos { namespace zones { class ZoneComposer; } }
namespace lightwaveos { namespace transitions { class TransitionEngine; enum class TransitionType : uint8_t; } }

namespace lightwaveos {
namespace actors {

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief LED strip configuration
 */
struct LedConfig {
    static constexpr uint16_t LEDS_PER_STRIP = 160;
    static constexpr uint16_t NUM_STRIPS = 2;
    static constexpr uint16_t TOTAL_LEDS = LEDS_PER_STRIP * NUM_STRIPS;  // 320

    static constexpr uint8_t STRIP1_PIN = 4;
    static constexpr uint8_t STRIP2_PIN = 5;

    static constexpr uint16_t TARGET_FPS = 120;
    static constexpr uint32_t FRAME_TIME_US = 1000000 / TARGET_FPS;  // ~8333us

    static constexpr uint8_t DEFAULT_BRIGHTNESS = 96;
    static constexpr uint8_t MAX_BRIGHTNESS = 160;
    static constexpr uint8_t DEFAULT_SPEED = 10;
    static constexpr uint8_t MAX_SPEED = 50;

    // Center origin point for effects
    static constexpr uint8_t CENTER_POINT = 79;  // LED 79/80 split
};

/**
 * @brief Render statistics
 */
struct RenderStats {
    uint32_t framesRendered;      // Total frames since start
    uint32_t frameDrops;          // Frames that exceeded budget
    uint32_t avgFrameTimeUs;      // Rolling average frame time
    uint32_t maxFrameTimeUs;      // Maximum frame time seen
    uint32_t minFrameTimeUs;      // Minimum frame time seen
    uint8_t currentFPS;           // Measured FPS
    uint8_t cpuPercent;           // CPU usage estimate

    RenderStats()
        : framesRendered(0), frameDrops(0)
        , avgFrameTimeUs(0), maxFrameTimeUs(0), minFrameTimeUs(UINT32_MAX)
        , currentFPS(0), cpuPercent(0) {}

    void reset() {
        framesRendered = 0;
        frameDrops = 0;
        avgFrameTimeUs = 0;
        maxFrameTimeUs = 0;
        minFrameTimeUs = UINT32_MAX;
        currentFPS = 0;
        cpuPercent = 0;
    }
};

/**
 * @brief Effect render function signature
 *
 * Effects are stateless functions that modify the LED buffer.
 * They receive a pointer to the buffer and the current state.
 */
struct RenderContext {
    CRGB* leds;                   // LED buffer (320 LEDs)
    uint16_t numLeds;             // Number of LEDs
    uint8_t brightness;           // Current brightness (0-255)
    uint8_t speed;                // Animation speed (1-50)
    uint8_t hue;                  // Global hue offset
    // Visual parameters (universal effect controls)
    uint8_t intensity;            // Effect intensity/amplitude (0-255)
    uint8_t saturation;           // Color saturation (0-255)
    uint8_t complexity;           // Effect complexity/detail (0-255)
    uint8_t variation;            // Effect variation/mode (0-255)
    uint32_t frameCount;          // Frame counter
    uint32_t deltaTimeMs;         // Time since last frame
    CRGBPalette16* palette;       // Current color palette
};

/**
 * @brief Effect render function type
 */
using EffectRenderFn = void (*)(RenderContext& ctx);

// ============================================================================
// RendererActor Class
// ============================================================================

/**
 * @brief Actor responsible for all LED rendering
 *
 * Runs on Core 1 at priority 5 (highest). The onTick() method is called
 * at ~120 FPS to render the current effect and push data to the strips.
 *
 * State changes (effect, brightness, etc.) are received as messages
 * and applied atomically before the next frame.
 */
class RendererActor : public Actor {
public:
    /**
     * @brief Construct the RendererActor
     *
     * Uses the predefined Renderer configuration from ActorConfigs.
     */
    RendererActor();

    /**
     * @brief Destructor
     */
    ~RendererActor() override;

    // ========================================================================
    // State Accessors (read-only, for diagnostics)
    // ========================================================================

    uint8_t getCurrentEffect() const { return m_currentEffect; }
    uint8_t getBrightness() const { return m_brightness; }
    uint8_t getSpeed() const { return m_speed; }
    uint8_t getPaletteIndex() const { return m_paletteIndex; }
    uint8_t getHue() const { return m_hue; }
    uint8_t getIntensity() const { return m_intensity; }
    uint8_t getSaturation() const { return m_saturation; }
    uint8_t getComplexity() const { return m_complexity; }
    uint8_t getVariation() const { return m_variation; }
    const RenderStats& getStats() const { return m_stats; }

    /**
     * @brief Get a copy of the current LED buffer
     *
     * Safe to call from other cores - copies the buffer atomically.
     *
     * @param outBuffer Buffer to copy into (must be TOTAL_LEDS size)
     */
    void getBufferCopy(CRGB* outBuffer) const;

    // ========================================================================
    // Effect Registration
    // ========================================================================

    /**
     * @brief Register an effect render function
     *
     * Effects should be registered before start().
     *
     * @param id Effect ID (0-255)
     * @param name Human-readable name
     * @param fn Render function
     * @return true if registered successfully
     */
    bool registerEffect(uint8_t id, const char* name, EffectRenderFn fn);

    /**
     * @brief Get number of registered effects
     */
    uint8_t getEffectCount() const { return m_effectCount; }

    /**
     * @brief Get effect name by ID
     */
    const char* getEffectName(uint8_t id) const;

    /**
     * @brief Get effect render function by ID
     * @param id Effect ID
     * @return Effect function pointer, or nullptr if not found
     */
    EffectRenderFn getEffectFunction(uint8_t id) const;

    /**
     * @brief Get pointer to current palette
     */
    CRGBPalette16* getPalette() { return &m_currentPalette; }

    /**
     * @brief Get pointer to LED buffer (for ZoneComposer)
     */
    CRGB* getLedBuffer() { return m_leds; }

    /**
     * @brief Get total number of available palettes
     */
    uint8_t getPaletteCount() const;

    /**
     * @brief Get palette name by ID
     * @param id Palette ID (0-74)
     * @return Palette name or "Unknown" if out of range
     */
    const char* getPaletteName(uint8_t id) const;

    // ========================================================================
    // Zone System Integration
    // ========================================================================

    /**
     * @brief Set the zone composer for multi-zone rendering
     * @param composer Pointer to ZoneComposer (nullptr to disable)
     */
    void setZoneComposer(zones::ZoneComposer* composer) { m_zoneComposer = composer; }

    /**
     * @brief Get the current zone composer
     */
    zones::ZoneComposer* getZoneComposer() { return m_zoneComposer; }

    // ========================================================================
    // Transition System Integration
    // ========================================================================

    /**
     * @brief Start a transition to a new effect
     * @param newEffectId Target effect ID
     * @param transitionType Transition type (0-11)
     */
    void startTransition(uint8_t newEffectId, uint8_t transitionType);

    /**
     * @brief Start transition with random type
     * @param newEffectId Target effect ID
     */
    void startRandomTransition(uint8_t newEffectId);

    /**
     * @brief Check if a transition is currently active
     */
    bool isTransitionActive() const;

    /**
     * @brief Get the transition engine (for external control)
     */
    transitions::TransitionEngine* getTransitionEngine() { return m_transitionEngine; }

    // ========================================================================
    // Frame Capture System (for testbed)
    // ========================================================================

    /**
     * @brief Frame capture tap points
     */
    enum class CaptureTap : uint8_t {
        TAP_A_PRE_CORRECTION = 0,   // After renderFrame(), before processBuffer()
        TAP_B_POST_CORRECTION = 1,  // After processBuffer(), before showLeds()
        TAP_C_PRE_WS2812 = 2        // After showLeds() copy, before FastLED.show()
    };

    /**
     * @brief Enable/disable frame capture mode
     * @param enabled True to enable capture, false to disable
     * @param tapMask Bitmask of taps to capture (bit 0=Tap A, bit 1=Tap B, bit 2=Tap C)
     */
    void setCaptureMode(bool enabled, uint8_t tapMask = 0x07);

    /**
     * @brief Check if capture mode is enabled
     */
    bool isCaptureModeEnabled() const { return m_captureEnabled; }

    /**
     * @brief Get captured frame for a specific tap
     * @param tap Tap point to retrieve
     * @param outBuffer Buffer to copy into (must be TOTAL_LEDS size)
     * @return true if frame was captured, false if not available
     */
    bool getCapturedFrame(CaptureTap tap, CRGB* outBuffer) const;

    /**
     * @brief Get capture metadata (effect ID, palette ID, frame index, timestamp)
     */
    struct CaptureMetadata {
        uint8_t effectId;
        uint8_t paletteId;
        uint8_t brightness;
        uint8_t speed;
        uint32_t frameIndex;
        uint32_t timestampUs;
    };
    CaptureMetadata getCaptureMetadata() const;

    /**
     * @brief Force a single render/capture cycle for the requested tap.
     *
     * This is intended for on-demand serial `capture dump` requests, to avoid
     * returning "No frame captured" when the caller requests a dump before the
     * next normal render tick has produced a captured frame.
     *
     * IMPORTANT: This method must not permanently mutate the live LED state
     * buffer used by buffer-feedback effects. It snapshots and restores `m_leds`.
     */
    void forceOneShotCapture(CaptureTap tap);

protected:
    // ========================================================================
    // Actor Overrides
    // ========================================================================

    void onStart() override;
    void onMessage(const Message& msg) override;
    void onTick() override;
    void onStop() override;

private:
    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Initialize FastLED and LED buffers
     */
    void initLeds();

    /**
     * @brief Render current effect to LED buffer
     */
    void renderFrame();

    /**
     * @brief Push LED buffer to physical strips
     */
    void showLeds();

    /**
     * @brief Update frame timing statistics
     */
    void updateStats(uint32_t frameTimeUs);

    /**
     * @brief Handle SET_EFFECT message
     */
    void handleSetEffect(uint8_t effectId);

    /**
     * @brief Handle SET_BRIGHTNESS message
     */
    void handleSetBrightness(uint8_t brightness);

    /**
     * @brief Handle SET_SPEED message
     */
    void handleSetSpeed(uint8_t speed);

    /**
     * @brief Handle SET_PALETTE message
     */
    void handleSetPalette(uint8_t paletteIndex);
    void handleSetIntensity(uint8_t intensity);
    void handleSetSaturation(uint8_t saturation);
    void handleSetComplexity(uint8_t complexity);
    void handleSetVariation(uint8_t variation);

    // ========================================================================
    // State
    // ========================================================================

    // LED buffers
    CRGB m_strip1[LedConfig::LEDS_PER_STRIP];
    CRGB m_strip2[LedConfig::LEDS_PER_STRIP];
    CRGB m_leds[LedConfig::TOTAL_LEDS];  // Unified buffer

    // Current state
    uint8_t m_currentEffect;
    uint8_t m_brightness;
    uint8_t m_speed;
    uint8_t m_paletteIndex;
    uint8_t m_hue;
    uint8_t m_intensity;
    uint8_t m_saturation;
    uint8_t m_complexity;
    uint8_t m_variation;

    // Palette
    CRGBPalette16 m_currentPalette;

    // Effect registry
    static constexpr uint8_t MAX_EFFECTS = 80;
    struct EffectEntry {
        const char* name;
        EffectRenderFn fn;
        bool active;
    };
    EffectEntry m_effects[MAX_EFFECTS];
    uint8_t m_effectCount;

    // Timing
    uint32_t m_lastFrameTime;
    uint32_t m_frameCount;

    // Statistics
    RenderStats m_stats;

    // Controller references
    CLEDController* m_ctrl1;
    CLEDController* m_ctrl2;

    // Zone system
    zones::ZoneComposer* m_zoneComposer;

    // Transition system
    transitions::TransitionEngine* m_transitionEngine;
    CRGB m_transitionSourceBuffer[LedConfig::TOTAL_LEDS];
    uint8_t m_pendingEffect;
    bool m_transitionPending;

    // Frame capture system (for testbed)
    bool m_captureEnabled;
    uint8_t m_captureTapMask;  // Bitmask: bit 0=Tap A, bit 1=Tap B, bit 2=Tap C
    
    // Performance metrics for color correction
    uint32_t m_correctionSkipCount;   // Number of frames where correction was skipped
    uint32_t m_correctionApplyCount;  // Number of frames where correction was applied
    CRGB m_captureTapA[LedConfig::TOTAL_LEDS];  // Pre-correction
    CRGB m_captureTapB[LedConfig::TOTAL_LEDS];  // Post-correction
    CRGB m_captureTapC[LedConfig::TOTAL_LEDS];  // Pre-WS2812 (per-strip, interleaved)
    CaptureMetadata m_captureMetadata;
    bool m_captureTapAValid;
    bool m_captureTapBValid;
    bool m_captureTapCValid;

    /**
     * @brief Capture frame at specified tap point
     * @param tap Tap point to capture
     * @param sourceBuffer Source buffer to copy from
     */
    void captureFrame(CaptureTap tap, const CRGB* sourceBuffer);
};

} // namespace actors
} // namespace lightwaveos
