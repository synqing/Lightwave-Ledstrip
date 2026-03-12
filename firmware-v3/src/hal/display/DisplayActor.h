/**
 * @file DisplayActor.h
 * @brief Actor for rendering test rig diagnostics to AMOLED display
 *
 * LightwaveOS v2 - AMOLED Test Rig Display
 *
 * Renders a 5-panel diagnostic view on the Waveshare 2.41" AMOLED (600×450):
 *
 *   ┌──────────────────────────────────────────────────┐
 *   │  STRIP 1 PREVIEW  (600 × 40px)                  │  y: 0-39
 *   │  160 LEDs → 600px, GPIO 4                        │
 *   ├──────────────────────────────────────────────────┤
 *   │  STRIP 2 PREVIEW  (600 × 40px)                  │  y: 40-79
 *   │  160 LEDs → 600px, GPIO 5                        │
 *   ├──────────────────────────────────────────────────┤
 *   │  SPACETIME HEATMAP (600 × 240px)                 │  y: 80-319
 *   │  320 LEDs × last 240 lines (~8s at 30fps)       │
 *   ├──────────────────────────────────────────────────┤
 *   │  METRICS BAR (600 × 90px)                        │  y: 320-409
 *   │  RMS | FLUX | BPM | BEAT | FPS | HEAP           │
 *   ├──────────────────────────────────────────────────┤
 *   │  STATUS LINE (600 × 40px)                        │  y: 410-449
 *   │  Effect name | Palette | Recording state         │
 *   └──────────────────────────────────────────────────┘
 *
 * Runs on Core 0 at priority 1 (lowest — informational only).
 * Ticks at 30 FPS (33ms interval).
 *
 * Data sources:
 *   - LED buffer: RendererActor::getBufferCopy()
 *   - Audio metrics: RendererActor::copyCachedAudioFrame()
 *   - Render stats: RendererActor::getStats()
 *   - Effect info: RendererActor::getCurrentEffect(), getEffectName()
 *
 * Thread safety:
 *   DisplayActor only READS from RendererActor via its thread-safe accessors.
 *   All AMOLED SPI transactions happen exclusively on this actor's task.
 */

#pragma once

#include "../../core/actors/Actor.h"
#include "../../config/features.h"
#include "RM690B0Driver.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>  // For CRGB type
#endif

// Forward declarations
namespace lightwaveos { namespace actors { class RendererActor; } }

namespace lightwaveos {
namespace display {

// ============================================================================
// Layout Constants
// ============================================================================

namespace layout {
    // Display dimensions
    constexpr uint16_t SCREEN_W = 600;
    constexpr uint16_t SCREEN_H = 450;

    // Panel geometry (y-offsets and heights)
    constexpr uint16_t STRIP1_Y      = 0;
    constexpr uint16_t STRIP1_H      = 40;

    constexpr uint16_t STRIP2_Y      = 40;
    constexpr uint16_t STRIP2_H      = 40;

    constexpr uint16_t HEATMAP_Y     = 80;
    constexpr uint16_t HEATMAP_H     = 240;

    constexpr uint16_t METRICS_Y     = 320;
    constexpr uint16_t METRICS_H     = 90;

    constexpr uint16_t STATUS_Y      = 410;
    constexpr uint16_t STATUS_H      = 40;

    // LED counts
    constexpr uint16_t LEDS_PER_STRIP = 160;
    constexpr uint16_t TOTAL_LEDS     = 320;

    // Strip preview: 160 LEDs scaled to 600px wide → ~3.75px per LED
    // Use integer scaling: 3px base + distribute remainder
    constexpr uint16_t LED_PIXEL_WIDTH = 3;  // Minimum pixels per LED
    // 600 - (160 * 3) = 120 extra pixels distributed across first 120 LEDs

    // Heatmap: 320 LEDs across 600px → ~1.875px per LED
    // Use 1px per LED (320px) centred, or 2px per LED (640px > 600, too wide)
    // Solution: 1px per LED, centred at x=140, leaving 140px margins
    constexpr uint16_t HEATMAP_LED_PX    = 1;
    constexpr uint16_t HEATMAP_X_OFFSET  = 140;  // (600 - 320) / 2
    constexpr uint16_t HEATMAP_LINES     = HEATMAP_H;  // 240 lines of history

} // namespace layout

// ============================================================================
// DisplayActor
// ============================================================================

class DisplayActor : public actors::Actor {
public:
    /**
     * @brief Construct DisplayActor
     * @param renderer Pointer to RendererActor for data access
     */
    explicit DisplayActor(actors::RendererActor* renderer);

    ~DisplayActor() override;

    // Non-copyable
    DisplayActor(const DisplayActor&) = delete;
    DisplayActor& operator=(const DisplayActor&) = delete;

protected:
    void onStart() override;
    void onMessage(const actors::Message& msg) override;
    void onTick() override;
    void onStop() override;

private:
    // ========================================================================
    // Panel Renderers
    // ========================================================================

    void renderStripPreview(uint16_t yOffset, const CRGB* leds, uint16_t count);
    void renderHeatmap();
    void renderMetrics();
    void renderStatusLine();

    // ========================================================================
    // Drawing Primitives
    // ========================================================================

    /**
     * @brief Convert CRGB to RGB565 (big-endian for display)
     */
    static uint16_t crgbToRgb565(const CRGB& c);

    /**
     * @brief Fill a rectangular region with a solid colour
     */
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color565);

    /**
     * @brief Draw a single character at (x, y) using BitmapFont
     * @return x advance (pixels consumed)
     */
    uint16_t drawChar(uint16_t x, uint16_t y, char c, uint16_t fg565, uint16_t bg565);

    /**
     * @brief Draw a string at (x, y)
     * @param scale Font scaling factor (1 = 5×7, 2 = 10×14)
     */
    void drawString(uint16_t x, uint16_t y, const char* str,
                    uint16_t fg565, uint16_t bg565, uint8_t scale = 1);

    /**
     * @brief Draw a horizontal bar graph
     * @param x, y, w, h  Bounding box
     * @param value  0.0 - 1.0
     * @param barColor  Colour of the filled portion
     * @param bgColor   Background colour
     */
    void drawBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 float value, uint16_t barColor, uint16_t bgColor);

    // ========================================================================
    // State
    // ========================================================================

    actors::RendererActor* m_renderer;
    RM690B0Driver m_display;

    // LED buffer snapshot (updated each tick)
    CRGB m_ledSnapshot[layout::TOTAL_LEDS];

    // Spacetime heatmap ring buffer
    // Stores RGB565 rows for the heatmap; newest at m_heatmapHead
    uint16_t m_heatmapBuf[layout::HEATMAP_LINES][layout::TOTAL_LEDS];
    uint16_t m_heatmapHead;  // Next write position (ring buffer index)

    // Scanline buffer for SPI transfers (one row of pixels)
    // Allocated in PSRAM if available
    uint16_t* m_scanline;
    static constexpr uint32_t SCANLINE_PIXELS = layout::SCREEN_W;

    // Frame counter for display update pacing
    uint32_t m_displayFrame;

    // Cached strings to avoid re-rendering unchanged text
    char m_lastEffectName[32];
    char m_lastStatusText[64];

    // Pre-computed RGB565 colours
    static constexpr uint16_t COLOR_BLACK   = 0x0000;
    static constexpr uint16_t COLOR_WHITE   = 0xFFFF;
    static constexpr uint16_t COLOR_GREEN   = 0x07E0;
    static constexpr uint16_t COLOR_RED     = 0xF800;
    static constexpr uint16_t COLOR_YELLOW  = 0xFFE0;
    static constexpr uint16_t COLOR_CYAN    = 0x07FF;
    static constexpr uint16_t COLOR_GREY    = 0x4208;  // ~25% grey
    static constexpr uint16_t COLOR_DKGREY  = 0x2104;  // ~12% grey
    static constexpr uint16_t COLOR_ORANGE  = 0xFD20;
};

} // namespace display
} // namespace lightwaveos
