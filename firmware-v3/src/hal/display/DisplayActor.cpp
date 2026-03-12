/**
 * @file DisplayActor.cpp
 * @brief DisplayActor implementation — 5-panel diagnostic display
 *
 * LightwaveOS v2 - AMOLED Test Rig
 *
 * Renders at 30 FPS on Core 0. Each tick:
 *   1. Snapshot LED buffer from RendererActor (thread-safe copy)
 *   2. Append new heatmap line to ring buffer
 *   3. Render all 5 panels to AMOLED via QSPI
 *
 * Memory budget:
 *   - LED snapshot:  320 × 3 = 960 bytes
 *   - Heatmap ring:  240 × 320 × 2 = 153,600 bytes (PSRAM)
 *   - Scanline buf:  600 × 2 = 1,200 bytes
 *   - Total: ~156 KB (comfortably fits in 8MB PSRAM)
 */

#include "DisplayActor.h"
#include "BitmapFont.h"
#include "../../core/actors/RendererActor.h"

#ifndef NATIVE_BUILD

#include <cstdio>
#include <cstring>
#include <esp_heap_caps.h>

namespace lightwaveos {
namespace display {

using namespace actors;
using namespace layout;

// ============================================================================
// Actor Configuration
// ============================================================================

static ActorConfig makeDisplayConfig() {
    return ActorConfig(
        "Display",      // name
        3072,           // stackSize (12KB) — SPI + rendering overhead
        1,              // priority (lowest — informational only)
        0,              // coreId (Core 0 — with network/audio)
        8,              // queueSize (small — mostly tick-driven)
        pdMS_TO_TICKS(33) // tickInterval (30 FPS)
    );
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

DisplayActor::DisplayActor(RendererActor* renderer)
    : Actor(makeDisplayConfig())
    , m_renderer(renderer)
    , m_heatmapHead(0)
    , m_scanline(nullptr)
    , m_displayFrame(0)
{
    memset(m_ledSnapshot, 0, sizeof(m_ledSnapshot));
    memset(m_heatmapBuf, 0, sizeof(m_heatmapBuf));
    memset(m_lastEffectName, 0, sizeof(m_lastEffectName));
    memset(m_lastStatusText, 0, sizeof(m_lastStatusText));
}

DisplayActor::~DisplayActor() {
    if (m_scanline) {
        heap_caps_free(m_scanline);
        m_scanline = nullptr;
    }
}

// ============================================================================
// Actor Lifecycle
// ============================================================================

void DisplayActor::onStart() {
    // Allocate scanline buffer (prefer PSRAM)
    m_scanline = static_cast<uint16_t*>(
        heap_caps_malloc(SCANLINE_PIXELS * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (!m_scanline) {
        // Fallback to internal RAM
        m_scanline = static_cast<uint16_t*>(
            heap_caps_malloc(SCANLINE_PIXELS * sizeof(uint16_t), MALLOC_CAP_8BIT)
        );
    }

    // Initialize display hardware
    if (!m_display.init()) {
        // Display init failed — actor will tick but skip rendering
        return;
    }

    // Clear screen to black
    m_display.setBrightness(200);
    fillRect(0, 0, SCREEN_W, SCREEN_H, COLOR_BLACK);
}

void DisplayActor::onMessage(const Message& msg) {
    // DisplayActor is tick-driven; messages are not used yet.
    // Future: could accept DISPLAY_BRIGHTNESS, DISPLAY_MODE commands.
    (void)msg;
}

void DisplayActor::onTick() {
    if (!m_display.isInitialized() || !m_renderer || !m_scanline) return;

    // 1. Snapshot LED buffer (thread-safe)
    m_renderer->getBufferCopy(m_ledSnapshot);

    // 2. Append to heatmap ring buffer
    for (uint16_t i = 0; i < TOTAL_LEDS; i++) {
        m_heatmapBuf[m_heatmapHead][i] = crgbToRgb565(m_ledSnapshot[i]);
    }
    m_heatmapHead = (m_heatmapHead + 1) % HEATMAP_LINES;

    // 3. Render panels
    renderStripPreview(STRIP1_Y, &m_ledSnapshot[0], LEDS_PER_STRIP);
    renderStripPreview(STRIP2_Y, &m_ledSnapshot[LEDS_PER_STRIP], LEDS_PER_STRIP);
    renderHeatmap();
    renderMetrics();
    renderStatusLine();

    m_displayFrame++;
}

void DisplayActor::onStop() {
    if (m_display.isInitialized()) {
        fillRect(0, 0, SCREEN_W, SCREEN_H, COLOR_BLACK);
        m_display.setDisplayOn(false);
    }
}

// ============================================================================
// Panel: Strip Preview
// ============================================================================

void DisplayActor::renderStripPreview(uint16_t yOffset, const CRGB* leds, uint16_t count) {
    // Render 160 LEDs across 600px width.
    // Each LED gets LED_PIXEL_WIDTH (3) pixels minimum.
    // 600 / 160 = 3.75, so 120 LEDs get 4px, remaining 40 get 3px.
    static constexpr uint16_t EXTRA_PIXELS = SCREEN_W - (LEDS_PER_STRIP * LED_PIXEL_WIDTH);
    // EXTRA_PIXELS = 600 - 480 = 120

    // Build one scanline, then replicate for strip height
    uint16_t xPos = 0;
    for (uint16_t led = 0; led < count && xPos < SCREEN_W; led++) {
        uint16_t color = crgbToRgb565(leds[led]);
        uint16_t pixelsForThisLed = LED_PIXEL_WIDTH + (led < EXTRA_PIXELS ? 1 : 0);

        for (uint16_t px = 0; px < pixelsForThisLed && xPos < SCREEN_W; px++) {
            m_scanline[xPos++] = color;
        }
    }
    // Fill any remaining pixels
    while (xPos < SCREEN_W) {
        m_scanline[xPos++] = COLOR_BLACK;
    }

    // Push the same scanline for each row of the strip preview
    // (Optimisation: set window once, push strip height × width pixels)
    m_display.setWindow(0, yOffset, SCREEN_W - 1, yOffset + layout::STRIP1_H - 1);

    for (uint16_t row = 0; row < layout::STRIP1_H; row++) {
        m_display.pushPixels(m_scanline, SCREEN_W);
    }
}

// ============================================================================
// Panel: Spacetime Heatmap
// ============================================================================

void DisplayActor::renderHeatmap() {
    // The heatmap shows 240 lines of history, newest at bottom.
    // Ring buffer: m_heatmapHead points to the NEXT write position,
    // so the oldest line is at m_heatmapHead, newest at m_heatmapHead-1.

    m_display.setWindow(0, HEATMAP_Y, SCREEN_W - 1, HEATMAP_Y + HEATMAP_H - 1);

    for (uint16_t row = 0; row < HEATMAP_H; row++) {
        // Map display row to ring buffer index (oldest at top)
        uint16_t bufIdx = (m_heatmapHead + row) % HEATMAP_LINES;

        // Build scanline: 320 LEDs centred in 600px
        // Left margin
        for (uint16_t x = 0; x < HEATMAP_X_OFFSET; x++) {
            m_scanline[x] = COLOR_BLACK;
        }

        // LED data (1px per LED)
        for (uint16_t led = 0; led < TOTAL_LEDS; led++) {
            m_scanline[HEATMAP_X_OFFSET + led] = m_heatmapBuf[bufIdx][led];
        }

        // Right margin
        for (uint16_t x = HEATMAP_X_OFFSET + TOTAL_LEDS; x < SCREEN_W; x++) {
            m_scanline[x] = COLOR_BLACK;
        }

        m_display.pushPixels(m_scanline, SCREEN_W);
    }
}

// ============================================================================
// Panel: Metrics Bar
// ============================================================================

void DisplayActor::renderMetrics() {
    // Clear metrics area
    fillRect(0, METRICS_Y, SCREEN_W, METRICS_H, COLOR_BLACK);

    // Get data from renderer
    const auto& stats = m_renderer->getStats();

#if FEATURE_AUDIO_SYNC
    audio::ControlBusFrame audioBus;
    m_renderer->copyCachedAudioFrame(audioBus);

    float rms   = audioBus.rms;
    float flux  = audioBus.flux;
    float bpm   = audioBus.tempoBpm;
    bool  beat  = audioBus.tempoBeatTick;
#else
    float rms = 0.0f, flux = 0.0f, bpm = 0.0f;
    bool beat = false;
#endif

    uint16_t fps = stats.currentFPS;
    uint32_t freeHeap = esp_get_free_heap_size() / 1024;  // KB

    // Layout: 6 metric cells, each 100px wide
    // Row 1 (y+8):  Labels
    // Row 2 (y+24): Values (2× scale)
    // Row 3 (y+50): Bar graphs

    struct MetricCell {
        const char* label;
        float value;
        float maxVal;
        uint16_t barColor;
        char valueBuf[12];
    };

    MetricCell cells[6] = {
        {"RMS",  rms,  1.0f, COLOR_GREEN,  {}},
        {"FLUX", flux, 1.0f, COLOR_CYAN,   {}},
        {"BPM",  bpm,  200.0f, COLOR_ORANGE, {}},
        {"BEAT", beat ? 1.0f : 0.0f, 1.0f, COLOR_RED, {}},
        {"FPS",  static_cast<float>(fps), 120.0f, COLOR_YELLOW, {}},
        {"HEAP", static_cast<float>(freeHeap), 300.0f, COLOR_WHITE, {}},
    };

    // Format value strings
    snprintf(cells[0].valueBuf, sizeof(cells[0].valueBuf), "%.2f", rms);
    snprintf(cells[1].valueBuf, sizeof(cells[1].valueBuf), "%.2f", flux);
    snprintf(cells[2].valueBuf, sizeof(cells[2].valueBuf), "%.0f", bpm);
    snprintf(cells[3].valueBuf, sizeof(cells[3].valueBuf), "%s", beat ? "HIT" : "---");
    snprintf(cells[4].valueBuf, sizeof(cells[4].valueBuf), "%u", fps);
    snprintf(cells[5].valueBuf, sizeof(cells[5].valueBuf), "%luK", (unsigned long)freeHeap);

    constexpr uint16_t CELL_W = 100;
    constexpr uint16_t BASE_Y = METRICS_Y;

    for (uint8_t i = 0; i < 6; i++) {
        uint16_t cx = i * CELL_W;

        // Label (1× scale, grey)
        drawString(cx + 4, BASE_Y + 4, cells[i].label, COLOR_GREY, COLOR_BLACK, 1);

        // Value (2× scale, white)
        drawString(cx + 4, BASE_Y + 18, cells[i].valueBuf, COLOR_WHITE, COLOR_BLACK, 2);

        // Bar graph (below value)
        float normVal = cells[i].value / cells[i].maxVal;
        if (normVal > 1.0f) normVal = 1.0f;
        if (normVal < 0.0f) normVal = 0.0f;
        drawBar(cx + 4, BASE_Y + 50, CELL_W - 8, 12, normVal, cells[i].barColor, COLOR_DKGREY);

        // Vertical separator (except last cell)
        if (i < 5) {
            fillRect(cx + CELL_W - 1, BASE_Y + 2, 1, METRICS_H - 4, COLOR_DKGREY);
        }
    }
}

// ============================================================================
// Panel: Status Line
// ============================================================================

void DisplayActor::renderStatusLine() {
    // Build status text
    char statusBuf[64];
    const char* effectName = m_renderer->getEffectName(m_renderer->getCurrentEffect());
    if (!effectName) effectName = "---";

    uint8_t paletteIdx = m_renderer->getPaletteIndex();
    bool capturing = m_renderer->isCaptureModeEnabled();

    snprintf(statusBuf, sizeof(statusBuf), "FX: %s  PAL: %u  %s",
             effectName, paletteIdx, capturing ? "[REC]" : "");

    // Only re-render if changed
    if (strncmp(statusBuf, m_lastStatusText, sizeof(m_lastStatusText)) == 0) {
        return;
    }
    strncpy(m_lastStatusText, statusBuf, sizeof(m_lastStatusText) - 1);

    // Clear and draw
    fillRect(0, STATUS_Y, SCREEN_W, STATUS_H, COLOR_DKGREY);
    drawString(8, STATUS_Y + 12, statusBuf, COLOR_WHITE, COLOR_DKGREY, 2);
}

// ============================================================================
// Drawing Primitives
// ============================================================================

uint16_t DisplayActor::crgbToRgb565(const CRGB& c) {
    // RGB888 → RGB565 (big-endian / MSB-first for display)
    uint16_t r5 = (c.r >> 3) & 0x1F;
    uint16_t g6 = (c.g >> 2) & 0x3F;
    uint16_t b5 = (c.b >> 3) & 0x1F;
    uint16_t rgb565 = (r5 << 11) | (g6 << 5) | b5;
    // Swap bytes for big-endian SPI transfer
    return (rgb565 >> 8) | (rgb565 << 8);
}

void DisplayActor::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color565) {
    if (!m_scanline || w == 0 || h == 0) return;

    // Clamp
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;

    // Fill scanline with colour
    uint16_t fillW = (w < SCANLINE_PIXELS) ? w : SCANLINE_PIXELS;
    for (uint16_t i = 0; i < fillW; i++) {
        m_scanline[i] = color565;
    }

    m_display.setWindow(x, y, x + w - 1, y + h - 1);
    for (uint16_t row = 0; row < h; row++) {
        m_display.pushPixels(m_scanline, w);
    }
}

uint16_t DisplayActor::drawChar(uint16_t x, uint16_t y, char c,
                                 uint16_t fg565, uint16_t bg565) {
    const uint8_t* glyph = BitmapFont::getGlyph(c);

    // Render 5×7 glyph pixel-by-pixel into scanline, push per row
    m_display.setWindow(x, y, x + BitmapFont::GLYPH_WIDTH - 1,
                        y + BitmapFont::GLYPH_HEIGHT - 1);

    for (uint8_t row = 0; row < BitmapFont::GLYPH_HEIGHT; row++) {
        for (uint8_t col = 0; col < BitmapFont::GLYPH_WIDTH; col++) {
            uint8_t colBits = pgm_read_byte(&glyph[col]);
            m_scanline[col] = (colBits & (1 << row)) ? fg565 : bg565;
        }
        m_display.pushPixels(m_scanline, BitmapFont::GLYPH_WIDTH);
    }

    return BitmapFont::ADVANCE;
}

void DisplayActor::drawString(uint16_t x, uint16_t y, const char* str,
                               uint16_t fg565, uint16_t bg565, uint8_t scale) {
    if (!str || scale == 0) return;

    if (scale == 1) {
        // Fast path: 1× scale, draw character by character
        while (*str) {
            drawChar(x, y, *str, fg565, bg565);
            x += BitmapFont::ADVANCE;
            str++;
        }
        return;
    }

    // Scaled rendering: render each character scaled
    uint16_t charW = BitmapFont::GLYPH_WIDTH * scale;
    uint16_t charH = BitmapFont::GLYPH_HEIGHT * scale;
    uint16_t advance = BitmapFont::ADVANCE * scale;

    while (*str) {
        const uint8_t* glyph = BitmapFont::getGlyph(*str);

        m_display.setWindow(x, y, x + charW - 1, y + charH - 1);

        for (uint8_t row = 0; row < BitmapFont::GLYPH_HEIGHT; row++) {
            // Build one scaled row into scanline
            uint16_t px = 0;
            for (uint8_t col = 0; col < BitmapFont::GLYPH_WIDTH; col++) {
                uint8_t colBits = pgm_read_byte(&glyph[col]);
                uint16_t color = (colBits & (1 << row)) ? fg565 : bg565;
                for (uint8_t sx = 0; sx < scale; sx++) {
                    m_scanline[px++] = color;
                }
            }
            // Push scaled row 'scale' times for Y scaling
            for (uint8_t sy = 0; sy < scale; sy++) {
                m_display.pushPixels(m_scanline, charW);
            }
        }

        // Draw 1-column gap (scaled)
        if (*(str + 1)) {
            // Gap between characters (scale pixels wide)
            m_display.setWindow(x + charW, y, x + advance - 1, y + charH - 1);
            for (uint16_t i = 0; i < (uint16_t)scale; i++) {
                m_scanline[i] = bg565;
            }
            for (uint16_t row = 0; row < charH; row++) {
                m_display.pushPixels(m_scanline, scale);
            }
        }

        x += advance;
        str++;
    }
}

void DisplayActor::drawBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            float value, uint16_t barColor, uint16_t bgColor) {
    uint16_t filledW = static_cast<uint16_t>(value * w);
    if (filledW > w) filledW = w;

    // Filled portion
    if (filledW > 0) {
        fillRect(x, y, filledW, h, barColor);
    }
    // Background portion
    if (filledW < w) {
        fillRect(x + filledW, y, w - filledW, h, bgColor);
    }
}

} // namespace display
} // namespace lightwaveos

#endif // !NATIVE_BUILD
