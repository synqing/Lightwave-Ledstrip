/**
 * @file main.cpp
 * @brief Voice Feasibility Harness — microWakeWord on Waveshare ESP32-S3 AMOLED
 *
 * SPH0645 mic → I2S → TFLite Micro → wake word detection → live AMOLED dashboard
 *
 * Uses microWakeWord (Kevin Ahrendt / OHF-Voice) with TFLite Micro inference
 * instead of ESP-SR. Supports custom wake words via .tflite model swap.
 *
 * Display shows: real-time VU meter, detection state, wake count, resources
 * I2S GPIO: BCLK=38, WS/LRCL=40, DIN/DOUT=39
 * Display: SH8601 QSPI AMOLED 600x450 (GPIO 9-14, RST=21)
 */

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <driver/i2s_std.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "wake_detector.h"
#include "hey_lightwave_model.h"
#include "esp_lcd_sh8601.h"

#ifdef ESP_SR_ENABLED
#include "sr_probe.h"
#include "sr_lifecycle.h"
#include "sr_command_window.h"
#endif

// ============================================================================
// Pin definitions (Waveshare ESP32-S3 2.41" AMOLED)
// ============================================================================
#ifndef I2S_BCLK_GPIO
#define I2S_BCLK_GPIO 38
#endif
#ifndef I2S_WS_GPIO
#define I2S_WS_GPIO 40
#endif
#ifndef I2S_DIN_GPIO
#define I2S_DIN_GPIO 39
#endif

#define LCD_CS_GPIO     9
#define LCD_PCLK_GPIO   10
#define LCD_DATA0_GPIO  11
#define LCD_DATA1_GPIO  12
#define LCD_DATA2_GPIO  13
#define LCD_DATA3_GPIO  14
#define LCD_RST_GPIO    21
#define LCD_H_RES       600
#define LCD_V_RES       450
#define LCD_BPP         16

// ============================================================================
// Display init commands (from Waveshare reference)
// ============================================================================
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFE, (uint8_t []){0x20}, 1, 0},
    {0x26, (uint8_t []){0x0A}, 1, 0},
    {0x24, (uint8_t []){0x80}, 1, 0},
    {0xFE, (uint8_t []){0x00}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0xC2, (uint8_t []){0x00}, 1, 10},
    {0x35, (uint8_t []){0x00}, 0, 0},
    {0x51, (uint8_t []){0x00}, 1, 10},
    {0x11, (uint8_t []){0x00}, 0, 80},
    {0x2A, (uint8_t []){0x00,0x10,0x01,0xD1}, 4, 0},
    {0x2B, (uint8_t []){0x00,0x00,0x02,0x57}, 4, 0},
    {0x29, (uint8_t []){0x00}, 0, 10},
    {0x36, (uint8_t []){0x30}, 1, 0},
    {0x51, (uint8_t []){0xFF}, 1, 0},
};

// ============================================================================
// RGB565 colours
// ============================================================================
#define COL_BLACK       0x0000
#define COL_DARK_BG     0x0841  // very dark grey
#define COL_DARK_BLUE   0x0019
#define COL_NAVY        0x000A
#define COL_BLUE        0x001F
#define COL_GREEN       0x07E0
#define COL_BRIGHT_GREEN 0x47E0
#define COL_YELLOW      0xFFE0
#define COL_ORANGE      0xFD20
#define COL_RED         0xF800
#define COL_WHITE       0xFFFF
#define COL_GREY        0x7BEF
#define COL_DARK_GREY   0x3186
#define COL_CYAN        0x07FF
#define COL_DARK_CYAN   0x0410
#define COL_PANEL_BG    0x10A2  // dark blue-grey panel background
#define COL_BAR_BG      0x2104  // darker bar track

// ============================================================================
// 8x8 bitmap font (printable ASCII 0x20-0x7F, public domain CP437 subset)
// ============================================================================
static const uint8_t font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // #
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // &
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // (
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ,
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // .
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // /
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 1
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 2
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 3
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 4
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 5
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 6
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 7
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 8
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 9
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // :
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ;
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // <
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // =
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // >
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // ?
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // @
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // A
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // B
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // C
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // D
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // E
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // F
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // G
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // H
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // I
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // J
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // K
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // L
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // O
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // P
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // Q
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // R
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // S
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // T
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // U
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // X
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // Y
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // Z
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // [
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // backslash
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // a
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // b
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // c
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, // d
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, // e
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, // f
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // g
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // h
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // i
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // j
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // k
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // l
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // n
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // o
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // p
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // q
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // r
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // s
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // t
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // u
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // w
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // x
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // y
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // z
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // }
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // DEL
};

// ============================================================================
// Globals
// ============================================================================
static i2s_chan_handle_t s_rxHandle = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;
static WakeDetector s_detector;

// I2S read chunk size (10ms at 16kHz = 160 samples)
static constexpr int kFeedChunkSamples = 160;

// Shared state between tasks (atomic/volatile)
static volatile int32_t s_micAmplitude = 0;     // Current RMS-ish amplitude from feed task
static volatile int32_t s_micPeak = 0;          // Peak amplitude (decays)
static volatile int s_wakeCount = 0;            // Wake word detection count
static volatile uint32_t s_lastWakeMs = 0;      // Timestamp of last wake detection
static volatile int s_feedsPerSec = 0;          // Feed rate counter
static volatile float s_wakeProb = 0.0f;        // Current detection probability
static volatile bool s_feedRunning = false;
static volatile bool s_feedStop = false;         // Signal feed task to exit cleanly
static TaskHandle_t s_feedTaskHandle = NULL;
#ifdef ESP_SR_ENABLED
static volatile bool s_wakeTriggersCmd = true;   // Wake word opens command window
static volatile bool s_cmdWindowRequested = false;
#endif

// ============================================================================
// Zone compositing system — anti-tearing via PSRAM offscreen buffers
//
// Instead of calling esp_lcd_panel_draw_bitmap() per character (190+ SPI
// transactions per frame), we composite into offscreen PSRAM buffers and
// flush each zone in a single draw_bitmap call (max 5 transactions/frame).
//
// Memory budget (PSRAM):
//   Zone 0 (title):    600 *  44 * 2 =  52,800 bytes
//   Zone 1 (VU):       600 *  92 * 2 = 110,400 bytes
//   Zone 2 (state):    600 * 122 * 2 = 146,400 bytes
//   Zone 3 (resource): 600 *  90 * 2 = 108,000 bytes
//   Zone 4 (info):     600 *  80 * 2 =  96,000 bytes
//   Strip (boot only): 600 *  10 * 2 =  12,000 bytes (static, DRAM)
//   TOTAL PSRAM:                       513,600 bytes (~502 KB / 7.9 MB = 6.2%)
// ============================================================================

#define NUM_ZONES 5

// Zone indices (semantic names for readability)
#define ZONE_TITLE    0
#define ZONE_VU       1
#define ZONE_STATE    2
#define ZONE_RESOURCE 3
#define ZONE_INFO     4

struct Zone {
    uint16_t *buf;       // PSRAM pixel buffer (RGB565, byte-swapped in buffer)
    int       screenY;   // Top edge on display (absolute Y coordinate)
    int       width;     // Always LCD_H_RES (600)
    int       height;    // Zone height in pixels
    bool      dirty;     // Needs flush to display
};

static Zone s_zones[NUM_ZONES];
static bool s_zonesReady = false;

// Small strip buffer for fillScreen (static DRAM — only used during boot)
#define STRIP_H 10
static uint16_t s_stripBuf[LCD_H_RES * STRIP_H];

static inline uint16_t swapBytes(uint16_t c) {
    return (c >> 8) | (c << 8);
}

// ---- Zone allocation and flush ----

static bool initZones() {
    // Zone layout: {screenY, height}
    // These cover y=0..449 with small gaps that stay black (separator lines
    // are rendered within zones that own them).
    //  Content bounds (verified against layout constants):
    //    Title:    y 8..35      (TITLE_Y=8, scale2; TITLE_Y+20=28, scale1)
    //    VU:       y 46..126    (VU_Y-14=46 "MIC LEVEL"; VU_LABEL_Y+20=126 separator)
    //    State:    y 134..216   (STATE_Y-16=134 label; STATE_Y+50=200, scale2+h=216)
    //    Resource: y 280..339   (RES_Y=280; RES_Y+44+16=340)
    //    Info:     y 370..417   (INFO_Y-10=370 sep; INFO_Y+30+8=418)
    const struct { int y; int h; } defs[NUM_ZONES] = {
        {   0,  44 },  // ZONE_TITLE:    title + subtitle (y 0..43)
        {  44,  92 },  // ZONE_VU:       "MIC LEVEL" label, VU bar, amp text, sep (y 44..135)
        { 134, 122 },  // ZONE_STATE:    "WAKE DETECT" label + state text (y 134..255)
        { 270,  90 },  // ZONE_RESOURCE: SRAM + PSRAM bars and labels (y 270..359)
        { 370,  80 },  // ZONE_INFO:     uptime + model line (y 370..449)
    };

    size_t totalBytes = 0;
    for (int i = 0; i < NUM_ZONES; i++) {
        s_zones[i].screenY = defs[i].y;
        s_zones[i].width   = LCD_H_RES;
        s_zones[i].height  = defs[i].h;
        s_zones[i].dirty   = false;

        size_t bytes = LCD_H_RES * defs[i].h * sizeof(uint16_t);
        s_zones[i].buf = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        if (!s_zones[i].buf) {
            Serial.printf("[ZONE] PSRAM alloc FAILED for zone %d (%u bytes)\n",
                          i, (unsigned)bytes);
            return false;
        }
        memset(s_zones[i].buf, 0, bytes);  // Clear to black
        totalBytes += bytes;
    }

    Serial.printf("[ZONE] %d zones allocated, %u bytes in PSRAM (%.1f KB)\n",
                  NUM_ZONES, (unsigned)totalBytes, totalBytes / 1024.0f);
    s_zonesReady = true;
    return true;
}

// Flush one zone to the display — exactly ONE esp_lcd_panel_draw_bitmap() call.
static void zoneFlush(int zi) {
    if (!s_panel || !s_zonesReady || zi < 0 || zi >= NUM_ZONES) return;
    Zone &z = s_zones[zi];
    if (!z.dirty) return;

    esp_lcd_panel_draw_bitmap(s_panel, 0, z.screenY,
                              z.width, z.screenY + z.height, z.buf);
    z.dirty = false;
}

// Flush all dirty zones (called once per frame after compositing)
static void zoneFlushAll() {
    for (int i = 0; i < NUM_ZONES; i++) {
        zoneFlush(i);
    }
}

// ---- Zone-local drawing primitives ----
// All coordinates are SCREEN-ABSOLUTE. Functions translate to zone-local
// buffer offsets internally. Out-of-zone pixels are clipped.

static void zoneFillRect(int zi, int x, int y, int w, int h, uint16_t color) {
    if (!s_zonesReady || zi < 0 || zi >= NUM_ZONES) return;
    Zone &z = s_zones[zi];
    uint16_t sw = swapBytes(color);

    // Translate to zone-local Y and clip
    int zy = y - z.screenY;
    if (zy < 0) { h += zy; zy = 0; }
    if (zy + h > z.height) h = z.height - zy;
    if (x < 0) { w += x; x = 0; }
    if (x + w > z.width) w = z.width - x;
    if (w <= 0 || h <= 0) return;

    for (int row = zy; row < zy + h; row++) {
        uint16_t *line = z.buf + row * z.width + x;
        for (int col = 0; col < w; col++) {
            line[col] = sw;
        }
    }
    z.dirty = true;
}

static void zoneClear(int zi, uint16_t color) {
    if (!s_zonesReady || zi < 0 || zi >= NUM_ZONES) return;
    Zone &z = s_zones[zi];
    uint16_t sw = swapBytes(color);
    int total = z.width * z.height;
    for (int i = 0; i < total; i++) z.buf[i] = sw;
    z.dirty = true;
}

static void zoneDrawChar(int zi, int x, int y, char c,
                         uint16_t fg, uint16_t bg, int scale) {
    if (!s_zonesReady || zi < 0 || zi >= NUM_ZONES) return;
    Zone &z = s_zones[zi];
    if (c < 0x20 || c > 0x7F) c = ' ';
    const uint8_t *glyph = font8x8[c - 0x20];

    uint16_t fgSw = swapBytes(fg);
    uint16_t bgSw = swapBytes(bg);
    int zy = y - z.screenY;  // zone-local Y

    for (int grow = 0; grow < 8; grow++) {
        uint8_t bits = glyph[grow];
        for (int sy = 0; sy < scale; sy++) {
            int py = zy + grow * scale + sy;
            if (py < 0 || py >= z.height) continue;
            uint16_t *line = z.buf + py * z.width;
            for (int gcol = 0; gcol < 8; gcol++) {
                uint16_t pix = (bits & (1 << gcol)) ? fgSw : bgSw;
                for (int sx = 0; sx < scale; sx++) {
                    int px = x + gcol * scale + sx;
                    if (px >= 0 && px < z.width) {
                        line[px] = pix;
                    }
                }
            }
        }
    }
    z.dirty = true;
}

static void zoneDrawString(int zi, int x, int y, const char *str,
                           uint16_t fg, uint16_t bg, int scale) {
    int cx = x;
    while (*str) {
        zoneDrawChar(zi, cx, y, *str, fg, bg, scale);
        cx += 8 * scale;
        str++;
    }
}

static void zoneDrawBar(int zi, int x, int y, int w, int h,
                        int value, int maxVal,
                        uint16_t fillColor, uint16_t trackColor) {
    if (maxVal <= 0) maxVal = 1;
    int fillW = (value * w) / maxVal;
    if (fillW > w) fillW = w;
    if (fillW < 0) fillW = 0;
    if (fillW > 0) zoneFillRect(zi, x, y, fillW, h, fillColor);
    if (fillW < w) zoneFillRect(zi, x + fillW, y, w - fillW, h, trackColor);
}

// ---- Legacy direct-to-display primitives (boot sequence only) ----
// Used before zone buffers are allocated and for fillScreen during init.

static void fillScreen(uint16_t color) {
    if (!s_panel) return;
    uint16_t sw = swapBytes(color);
    for (int i = 0; i < LCD_H_RES * STRIP_H; i++) s_stripBuf[i] = sw;
    for (int y = 0; y < LCD_V_RES; y += STRIP_H) {
        int h = (y + STRIP_H <= LCD_V_RES) ? STRIP_H : (LCD_V_RES - y);
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + h, s_stripBuf);
    }
}

static void fillRect(int x, int y, int w, int h, uint16_t color) {
    if (!s_panel || w <= 0 || h <= 0) return;
    uint16_t sw = swapBytes(color);
    for (int row = y; row < y + h; row += STRIP_H) {
        int sh = (row + STRIP_H <= y + h) ? STRIP_H : (y + h - row);
        int pixels = w * sh;
        if (pixels > LCD_H_RES * STRIP_H) pixels = LCD_H_RES * STRIP_H;
        for (int i = 0; i < pixels; i++) s_stripBuf[i] = sw;
        esp_lcd_panel_draw_bitmap(s_panel, x, row, x + w, row + sh, s_stripBuf);
    }
}

static void drawChar(int x, int y, char c, uint16_t fg, uint16_t bg, int scale) {
    if (!s_panel) return;
    if (c < 0x20 || c > 0x7F) c = ' ';
    const uint8_t *glyph = font8x8[c - 0x20];
    int cw = 8 * scale;
    int ch = 8 * scale;
    if (cw * ch > LCD_H_RES * STRIP_H) return;

    uint16_t fgSw = swapBytes(fg);
    uint16_t bgSw = swapBytes(bg);

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int sy = 0; sy < scale; sy++) {
            int bufY = row * scale + sy;
            for (int col = 0; col < 8; col++) {
                uint16_t pix = (bits & (1 << col)) ? fgSw : bgSw;
                for (int sx = 0; sx < scale; sx++) {
                    s_stripBuf[bufY * cw + col * scale + sx] = pix;
                }
            }
        }
    }
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + cw, y + ch, s_stripBuf);
}

static void drawString(int x, int y, const char *str, uint16_t fg, uint16_t bg, int scale) {
    int cx = x;
    while (*str) {
        drawChar(cx, y, *str, fg, bg, scale);
        cx += 8 * scale;
        str++;
    }
}

// ============================================================================
// Display init
// ============================================================================
static bool initDisplay() {
    Serial.println("[LCD] Init SH8601 QSPI AMOLED...");

    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        LCD_PCLK_GPIO, LCD_DATA0_GPIO, LCD_DATA1_GPIO,
        LCD_DATA2_GPIO, LCD_DATA3_GPIO,
        LCD_H_RES * LCD_V_RES * LCD_BPP / 8
    );
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        Serial.printf("[LCD] SPI bus init FAIL: %s\n", esp_err_to_name(err));
        return false;
    }

    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        LCD_CS_GPIO, NULL, NULL
    );

    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };

    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle);
    if (err != ESP_OK) {
        Serial.printf("[LCD] Panel IO init FAIL: %s\n", esp_err_to_name(err));
        return false;
    }

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BPP,
        .vendor_config = &vendor_config,
    };

    err = esp_lcd_new_panel_sh8601(io_handle, &panel_config, &s_panel);
    if (err != ESP_OK) {
        Serial.printf("[LCD] Panel create FAIL: %s\n", esp_err_to_name(err));
        return false;
    }

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_disp_on_off(s_panel, true);

    fillScreen(COL_BLACK);
    Serial.println("[LCD] Display OK - 600x450 QSPI AMOLED");
    return true;
}

// ============================================================================
// Dashboard layout constants
// ============================================================================
#define MARGIN      16
#define TITLE_Y     8
#define VU_Y        60
#define VU_H        40
#define VU_LABEL_Y  (VU_Y + VU_H + 6)
#define STATE_Y     150
#define RES_Y       280
#define INFO_Y      380
#define BAR_W       (LCD_H_RES - MARGIN * 2 - 160)

// Draw the static parts of the dashboard into zone buffers (called once).
// Each zone is composited fully, then flushed in a single SPI transaction.
static void drawDashboardStatic() {
    if (!s_panel || !s_zonesReady) return;

    // Clear the physical screen first (direct draw, before zone flush)
    fillScreen(COL_BLACK);

    // --- Zone 0: Title ---
    zoneClear(ZONE_TITLE, COL_BLACK);
    zoneDrawString(ZONE_TITLE, MARGIN, TITLE_Y, "VOICE HARNESS v0.4", COL_CYAN, COL_BLACK, 2);
    zoneDrawString(ZONE_TITLE, MARGIN, TITLE_Y + 20, "Waveshare ESP32-S3 AMOLED", COL_DARK_GREY, COL_BLACK, 1);
    zoneFlush(ZONE_TITLE);

    // --- Zone 1: VU (static elements — bar track, labels) ---
    zoneClear(ZONE_VU, COL_BLACK);
    // "MIC LEVEL" label at top of zone
    zoneDrawString(ZONE_VU, MARGIN, VU_Y - 14, "MIC LEVEL", COL_GREY, COL_BLACK, 1);
    // VU track background
    zoneFillRect(ZONE_VU, MARGIN, VU_Y, LCD_H_RES - MARGIN * 2, VU_H, COL_BAR_BG);
    // Separator at bottom of zone
    zoneFillRect(ZONE_VU, MARGIN, VU_LABEL_Y + 20, LCD_H_RES - MARGIN * 2, 1, COL_DARK_GREY);
    zoneFlush(ZONE_VU);

    // --- Zone 2: State (initial state) ---
    zoneClear(ZONE_STATE, COL_BLACK);
    zoneDrawString(ZONE_STATE, MARGIN, STATE_Y - 16, "WAKE DETECT", COL_GREY, COL_BLACK, 1);
    zoneDrawString(ZONE_STATE, MARGIN + 80, STATE_Y + 10, "LISTENING...", COL_CYAN, COL_BLACK, 3);
    zoneDrawString(ZONE_STATE, MARGIN + 80, STATE_Y + 50, "Say \"Hey Lightwave\"", COL_DARK_GREY, COL_BLACK, 2);
    zoneFlush(ZONE_STATE);

    // --- Zone 3: Resources — intentionally blank (removed SRAM/PSRAM display) ---
    zoneClear(ZONE_RESOURCE, COL_BLACK);
    zoneFlush(ZONE_RESOURCE);

    // --- Zone 4: Info (separator + static model line) ---
    zoneClear(ZONE_INFO, COL_BLACK);
    zoneFillRect(ZONE_INFO, MARGIN, INFO_Y - 10, LCD_H_RES - MARGIN * 2, 1, COL_DARK_GREY);
    zoneDrawString(ZONE_INFO, MARGIN, INFO_Y + 30,
                   "MODEL: hey_lightwave (microWakeWord)  RATE: 16kHz",
                   COL_DARK_GREY, COL_BLACK, 1);
    zoneFlush(ZONE_INFO);
}

// ============================================================================
// Dashboard update — zone-composited, dirty-flag optimised
//
// Rendering flow per frame:
//   1. Compute new values (amplitude, state, heap, uptime)
//   2. For each zone whose content changed, composite into PSRAM buffer
//   3. Call zoneFlushAll() — only dirty zones get one draw_bitmap each
//
// Result: max 5 SPI transactions per frame (typically 1-2), down from ~190.
// Zero tearing because each zone arrives at the display as a single
// fully-composited bitmap — no visible intermediate states.
// ============================================================================

static struct {
    // VU zone: track previous fill width to avoid redundant recomposite
    int prevAmpFill;
    int prevPeakFill;
    uint16_t prevVuCol;
    char prevAmpText[64];

    // State zone: only recomposite on state transitions
    bool prevWakeRecent;
    int prevStateWakeCount;

    // Info zone: throttled to 1 Hz
    uint32_t lastInfoMs;
    char prevInfoText[64];
} s_dirty = {};

static void updateDashboard() {
    if (!s_panel || !s_zonesReady) return;

    char buf[64];
    uint32_t now = millis();

    // ---- ZONE 1: VU meter ----
    // The VU bar changes every frame during audio. We recomposite the entire
    // zone into the buffer, then flush once. The compositing cost is trivial
    // (~96 KB of PSRAM writes at CPU speed) vs the eliminated SPI overhead.
    {
        int32_t amp = s_micAmplitude;
        int32_t peak = s_micPeak;
        int barW = LCD_H_RES - MARGIN * 2;
        int ampFill = (amp * barW) / 20000;
        if (ampFill > barW) ampFill = barW;
        if (ampFill < 0) ampFill = 0;
        int peakFill = (peak * barW) / 20000;
        if (peakFill > barW) peakFill = barW;

        uint16_t vuCol;
        int pct = barW > 0 ? (ampFill * 100) / barW : 0;
        if (pct < 40) vuCol = COL_GREEN;
        else if (pct < 70) vuCol = COL_YELLOW;
        else if (pct < 90) vuCol = COL_ORANGE;
        else vuCol = COL_RED;

        // Build amplitude text
        snprintf(buf, sizeof(buf), "AMP: %-6ld  PEAK: %-6ld  FEEDS/s: %-4d",
                 (long)amp, (long)peak, (int)s_feedsPerSec);

        // Check if anything in the VU zone actually changed
        bool vuDirty = (ampFill != s_dirty.prevAmpFill) ||
                       (peakFill != s_dirty.prevPeakFill) ||
                       (vuCol != s_dirty.prevVuCol) ||
                       (strcmp(buf, s_dirty.prevAmpText) != 0);

        if (vuDirty) {
            // Recomposite entire VU zone from scratch
            // (Faster than incremental rect patching in a memory buffer —
            //  the bottleneck was SPI, not CPU pixel writes.)

            // Clear zone to black
            zoneClear(ZONE_VU, COL_BLACK);

            // "MIC LEVEL" label
            zoneDrawString(ZONE_VU, MARGIN, VU_Y - 14, "MIC LEVEL", COL_GREY, COL_BLACK, 1);

            // VU bar: filled portion
            if (ampFill > 0)
                zoneFillRect(ZONE_VU, MARGIN, VU_Y, ampFill, VU_H, vuCol);
            // VU bar: empty track
            if (ampFill < barW)
                zoneFillRect(ZONE_VU, MARGIN + ampFill, VU_Y, barW - ampFill, VU_H, COL_BAR_BG);
            // Peak indicator (thin white line)
            if (peakFill > 2 && peakFill < barW)
                zoneFillRect(ZONE_VU, MARGIN + peakFill - 2, VU_Y, 3, VU_H, COL_WHITE);

            // Amplitude text
            zoneDrawString(ZONE_VU, MARGIN, VU_LABEL_Y, buf, COL_WHITE, COL_BLACK, 1);

            // Separator at bottom
            zoneFillRect(ZONE_VU, MARGIN, VU_LABEL_Y + 20, LCD_H_RES - MARGIN * 2, 1, COL_DARK_GREY);

            s_dirty.prevAmpFill = ampFill;
            s_dirty.prevPeakFill = peakFill;
            s_dirty.prevVuCol = vuCol;
            memcpy(s_dirty.prevAmpText, buf, sizeof(s_dirty.prevAmpText));
        }
    }

    // ---- ZONE 2: State indicator ----
    // Only recomposites on actual state transitions (wake detected / timeout).
    // This is the biggest tearing fix: the old code did fillRect(black) +
    // drawString every frame = visible black flash. Now the zone buffer is
    // composited atomically and flushed once.
    {
        bool wakeRecent = (s_lastWakeMs > 0 && (now - s_lastWakeMs) < 2000);
        int curWakeCount = s_wakeCount;
        bool stateDirty = (wakeRecent != s_dirty.prevWakeRecent) ||
                          (curWakeCount != s_dirty.prevStateWakeCount);

        if (stateDirty) {
            zoneClear(ZONE_STATE, COL_BLACK);
            zoneDrawString(ZONE_STATE, MARGIN, STATE_Y - 16, "WAKE DETECT", COL_GREY, COL_BLACK, 1);

            if (wakeRecent) {
                snprintf(buf, sizeof(buf), "WAKE #%d DETECTED!", curWakeCount);
                zoneDrawString(ZONE_STATE, MARGIN + 40, STATE_Y + 10, buf, COL_GREEN, COL_BLACK, 3);
                zoneDrawString(ZONE_STATE, MARGIN + 40, STATE_Y + 50,
                               "DETECTED!", COL_BRIGHT_GREEN, COL_BLACK, 2);
            } else {
                zoneDrawString(ZONE_STATE, MARGIN + 80, STATE_Y + 10,
                               "LISTENING...", COL_CYAN, COL_BLACK, 3);
                if (curWakeCount > 0) {
                    snprintf(buf, sizeof(buf), "Total wakes: %d", curWakeCount);
                    zoneDrawString(ZONE_STATE, MARGIN + 80, STATE_Y + 50,
                                   buf, COL_DARK_GREY, COL_BLACK, 2);
                } else {
                    zoneDrawString(ZONE_STATE, MARGIN + 80, STATE_Y + 50,
                                   "Say \"Hey Lightwave\"", COL_DARK_GREY, COL_BLACK, 2);
                }
            }

            s_dirty.prevWakeRecent = wakeRecent;
            s_dirty.prevStateWakeCount = curWakeCount;
        }
    }

    // ---- ZONE 3: Resource bars — removed (SRAM/PSRAM not needed for voice testing) ----

    // ---- ZONE 4: Bottom info (throttled to 1 Hz) ----
    if (now - s_dirty.lastInfoMs >= 1000) {
        s_dirty.lastInfoMs = now;

        uint32_t uptimeSec = now / 1000;
        int hrs = uptimeSec / 3600;
        int mins = (uptimeSec % 3600) / 60;
        int secs = uptimeSec % 60;
        snprintf(buf, sizeof(buf), "UP:%02d:%02d:%02d  WAKES:%d  F:%s",
                 hrs, mins, secs, s_wakeCount,
                 s_feedRunning ? "OK" : "--");

        if (strcmp(buf, s_dirty.prevInfoText) != 0) {
            // Recomposite info zone (preserve separator and model line)
            zoneClear(ZONE_INFO, COL_BLACK);
            zoneFillRect(ZONE_INFO, MARGIN, INFO_Y - 10,
                         LCD_H_RES - MARGIN * 2, 1, COL_DARK_GREY);
            zoneDrawString(ZONE_INFO, MARGIN, INFO_Y, buf, COL_GREY, COL_BLACK, 2);
            zoneDrawString(ZONE_INFO, MARGIN, INFO_Y + 30,
                           "MODEL: hey_lightwave (microWakeWord)  RATE: 16kHz",
                           COL_DARK_GREY, COL_BLACK, 1);
            memcpy(s_dirty.prevInfoText, buf, sizeof(s_dirty.prevInfoText));
        }
    }

    // ---- FLUSH all dirty zones to display ----
    // This is where the SPI transactions happen. Only zones whose content
    // changed above will have dirty=true. Typical frame: 1 zone (VU only).
    // Worst case (state transition + 1 Hz tick): 4 zones = 4 SPI transactions.
    zoneFlushAll();
}

// ============================================================================
// Resource reporting (serial)
// ============================================================================
static void printResources(const char* label) {
    size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t totalInt = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t totalPSRAM = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    Serial.printf("[RES %s] SRAM: %u/%u free | PSRAM: %u/%u free\n",
                  label, freeInt, totalInt, freePSRAM, totalPSRAM);
}

// ============================================================================
// I2S Microphone
// ============================================================================
static bool initMicrophone() {
    Serial.println("[MIC] Init I2S (IDF 5.x channel API)...");
    Serial.printf("  BCLK=%d  WS=%d  DIN=%d\n", I2S_BCLK_GPIO, I2S_WS_GPIO, I2S_DIN_GPIO);

    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.dma_desc_num = 4;
    chanCfg.dma_frame_num = 512;

    esp_err_t err = i2s_new_channel(&chanCfg, NULL, &s_rxHandle);
    if (err != ESP_OK) {
        Serial.printf("[MIC] i2s_new_channel FAIL: %s\n", esp_err_to_name(err));
        return false;
    }

    i2s_std_config_t stdCfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCLK_GPIO,
            .ws   = (gpio_num_t)I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)I2S_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    stdCfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;

    err = i2s_channel_init_std_mode(s_rxHandle, &stdCfg);
    if (err != ESP_OK) {
        Serial.printf("[MIC] i2s_channel_init_std_mode FAIL: %s\n", esp_err_to_name(err));
        return false;
    }

    err = i2s_channel_enable(s_rxHandle);
    if (err != ESP_OK) {
        Serial.printf("[MIC] i2s_channel_enable FAIL: %s\n", esp_err_to_name(err));
        return false;
    }

    int32_t testBuf[512];
    size_t bytesRead = 0;
    i2s_channel_read(s_rxHandle, testBuf, sizeof(testBuf), &bytesRead, pdMS_TO_TICKS(500));

    int nonZero = 0;
    int32_t maxAbs = 0;
    for (int i = 0; i < 512; i++) {
        int32_t v = testBuf[i] >> 10;
        if (v != 0) nonZero++;
        int32_t a = abs(v);
        if (a > maxAbs) maxAbs = a;
    }

    Serial.printf("[MIC] Signal test: %d/512 non-zero, max amplitude: %ld\n", nonZero, (long)maxAbs);
    if (nonZero < 10) {
        Serial.println("[MIC] WARNING: Signal appears silent.");
    } else {
        Serial.println("[MIC] Signal OK.");
    }

    return true;
}

// ============================================================================
// Feed task — reads mic, computes amplitude, feeds wake word detector
// ============================================================================
static void feedTask(void *arg) {
    Serial.println("[FEED] Task started on core " + String(xPortGetCoreID()));
    s_feedRunning = true;
    s_feedStop = false;

    int32_t *rawBuf = (int32_t *)heap_caps_malloc(kFeedChunkSamples * sizeof(int32_t), MALLOC_CAP_INTERNAL);
    int16_t *feedBuf = (int16_t *)heap_caps_malloc(kFeedChunkSamples * sizeof(int16_t), MALLOC_CAP_INTERNAL);

    if (!rawBuf || !feedBuf) {
        Serial.println("[FEED] malloc FAIL");
        s_feedRunning = false;
        s_feedTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Drain stale I2S DMA data (prevents stale audio from hitting wake detector
    // after command window resume)
    for (int d = 0; d < 6; d++) {
        size_t discard = 0;
        i2s_channel_read(s_rxHandle, rawBuf, kFeedChunkSamples * sizeof(int32_t),
                         &discard, pdMS_TO_TICKS(50));
    }

    uint32_t feedCount = 0;
    uint32_t lastRateMs = millis();

    while (!s_feedStop) {
        size_t bytesRead = 0;
        esp_err_t err = i2s_channel_read(s_rxHandle, rawBuf,
                                          kFeedChunkSamples * sizeof(int32_t),
                                          &bytesRead, pdMS_TO_TICKS(200));
        if (err != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int samplesRead = bytesRead / sizeof(int32_t);

        // Convert to int16 and compute amplitude
        int32_t sumAbs = 0;
        int32_t maxAmp = 0;
        for (int i = 0; i < samplesRead; i++) {
            int16_t s = (int16_t)(rawBuf[i] >> 14);
            feedBuf[i] = s;
            int32_t a = abs((int32_t)s);
            sumAbs += a;
            if (a > maxAmp) maxAmp = a;
        }

        // Update shared amplitude (mean absolute value)
        if (samplesRead > 0) {
            s_micAmplitude = sumAbs / samplesRead;
        }
        // Peak with decay
        if (maxAmp > s_micPeak) {
            s_micPeak = maxAmp;
        } else {
            int32_t p = s_micPeak;
            p = p - (p >> 6);  // slow decay (~1.5% per chunk)
            if (p < 0) p = 0;
            s_micPeak = p;
        }

        // Feed samples to wake word detector (detection happens inside)
        s_detector.feedSamples(feedBuf, samplesRead);
        s_wakeProb = s_detector.currentProbability();

        // Check for detection (2-second cooldown to prevent double-counting)
        if (s_detector.detected() && (millis() - s_lastWakeMs > 2000)) {
            s_wakeCount++;
            s_lastWakeMs = millis();
            Serial.printf("\n[WAKE] >>> WAKE WORD DETECTED (#%d) <<< prob=%.3f\n",
                          s_wakeCount, s_detector.currentProbability());
            printResources("WAKE");
            s_detector.reset();
#ifdef ESP_SR_ENABLED
            // Don't trigger command mode within 5s of boot (avoids false triggers
            // from serial connection DTR/RTS toggling the mic)
            if (s_wakeTriggersCmd && millis() > 5000) {
                s_cmdWindowRequested = true;
            }
#endif
        } else if (s_detector.detected()) {
            s_detector.reset();  // Suppress duplicate, clear state
        }

        // Feed rate counter
        feedCount++;
        uint32_t now = millis();
        if (now - lastRateMs >= 1000) {
            s_feedsPerSec = feedCount;
            feedCount = 0;
            lastRateMs = now;
        }
    }

    // Clean exit — free buffers, clear state
    heap_caps_free(rawBuf);
    heap_caps_free(feedBuf);
    s_feedRunning = false;
    s_feedsPerSec = 0;
    s_feedTaskHandle = NULL;
    Serial.println("[FEED] Task stopped cleanly.");
    vTaskDelete(NULL);
}

// ============================================================================
// microWakeWord init
// ============================================================================
static bool initVoiceEngine() {
    Serial.println("[VOICE] Initialising microWakeWord (TFLite Micro)...");

    // Hey Lightwave v2 — 61 KB, adversarial-trained via microWakeWord
    bool ok = s_detector.begin(
        hey_lightwave_tflite,        // model data (from hey_lightwave_model.h)
        32000,                       // tensor arena size
        0.80f,                       // probability cutoff (90% TPR @ 0.187 FA/hr)
        5,                           // sliding window size
        10                           // feature step ms
    );

    if (!ok) {
        Serial.println("[VOICE] WakeDetector init FAILED");
        return false;
    }

    printResources("After WakeDetector init");

    // Single feed task — reads I2S and feeds detector (detection inline)
    xTaskCreatePinnedToCore(feedTask, "mww_feed", 16384, NULL, 3, &s_feedTaskHandle, 0);

    Serial.println("[VOICE] Feed task launched.");
    return true;
}

// ============================================================================
// Feed task stop/start helpers (used by Gate 3B lifecycle churn)
// ============================================================================
static bool stopFeedTask(int timeoutMs = 2000) {
    if (!s_feedRunning) return true;
    s_feedStop = true;
    uint32_t t0 = millis();
    while (s_feedRunning && (millis() - t0 < (uint32_t)timeoutMs)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return !s_feedRunning;
}

static bool startFeedTask() {
    if (s_feedRunning) return true;
    s_feedStop = false;
    xTaskCreatePinnedToCore(feedTask, "mww_feed", 16384, NULL, 3, &s_feedTaskHandle, 0);
    // Wait for task to signal running
    uint32_t t0 = millis();
    while (!s_feedRunning && (millis() - t0 < 2000)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return s_feedRunning;
}

static void teardownMicrophone() {
    if (s_rxHandle) {
        i2s_channel_disable(s_rxHandle);
        i2s_del_channel(s_rxHandle);
        s_rxHandle = NULL;
    }
}

#ifdef ESP_SR_ENABLED
// ============================================================================
// Gate 4 — Visual feedback for command window
// ============================================================================
static void cmdVisualHandler(CmdVisualState state, const char *detail) {
    // Serial-only feedback for Gate 4A — no display changes
    switch (state) {
    case CMD_VIS_WAKE_ACK:   Serial.println("[CMD_VIS] WAKE ACK"); break;
    case CMD_VIS_LISTENING:  Serial.println("[CMD_VIS] LISTENING"); break;
    case CMD_VIS_SUCCESS:    Serial.printf("[CMD_VIS] SUCCESS: %s\n", detail ? detail : "?"); break;
    case CMD_VIS_TIMEOUT:    Serial.println("[CMD_VIS] TIMEOUT"); break;
    case CMD_VIS_ERROR:      Serial.printf("[CMD_VIS] ERROR: %s\n", detail ? detail : "?"); break;
    case CMD_VIS_IDLE:       Serial.println("[CMD_VIS] IDLE"); break;
    }
}

// ============================================================================
// Gate 4 — Command action handler
// ============================================================================
static void cmdActionHandler(int commandId) {
    switch (commandId) {
    case CMD_LIGHTS_OFF:
        ESP_LOGI("CMD_ACT", "LIGHTS OFF — filling screen black");
        // For now, just darken the status area as visual proof
        // (In production, this would control the LED strips)
        Serial.println("[ACTION] LIGHTS OFF");
        break;
    case CMD_BRIGHTER:
        ESP_LOGI("CMD_ACT", "BRIGHTER — brightness step up");
        Serial.println("[ACTION] BRIGHTER (+25%)");
        break;
    default:
        ESP_LOGW("CMD_ACT", "Unknown command id=%d", commandId);
        break;
    }
}
#endif // ESP_SR_ENABLED

// ============================================================================
// Main
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("  Voice Feasibility Harness v1.0");
    Serial.println("  Waveshare ESP32-S3 2.41\" AMOLED");
    Serial.println("  microWakeWord — \"Hey Lightwave\"");
    Serial.println("========================================\n");

    printResources("BOOT");

#ifdef ESP_SR_ENABLED
    // Gate 1: Prove ESP-SR compiles, links, and can read model partition
    sr_probe_models();
    printResources("POST_SR_PROBE");
#endif

    bool lcdOk = initDisplay();
    if (lcdOk) {
        drawString(200, 200, "BOOTING...", COL_CYAN, COL_BLACK, 3);

        // Allocate zone compositing buffers in PSRAM
        if (!initZones()) {
            Serial.println("[LCD] Zone init failed — falling back to direct draw (tearing)");
        }
    }
    printResources("After LCD init");

    bool micOk = initMicrophone();
    printResources("After MIC init");

    if (!micOk) {
        Serial.println("[FATAL] Mic init failed.");
        if (lcdOk) {
            fillScreen(COL_BLACK);
            drawString(100, 180, "MIC INIT FAILED", COL_RED, COL_BLACK, 3);
            drawString(100, 220, "Check wiring: BCLK=38 WS=40 DIN=39", COL_GREY, COL_BLACK, 2);
        }
        while (true) { delay(1000); }
    }

    bool voiceOk = initVoiceEngine();
    printResources("After MWW init");

    if (!voiceOk) {
        Serial.println("[FATAL] microWakeWord init failed.");
        if (lcdOk) {
            fillScreen(COL_BLACK);
            drawString(100, 180, "MWW INIT FAILED", COL_RED, COL_BLACK, 3);
            drawString(100, 220, "Check TFLite model", COL_GREY, COL_BLACK, 2);
        }
        while (true) { delay(1000); }
    }

    if (lcdOk) {
        drawDashboardStatic();
    }

#ifdef ESP_SR_ENABLED
    // Register Gate 3B callbacks for I2S handoff churn
    sr_lifecycle_setCallbacks({
        .stopFeed    = stopFeedTask,
        .startFeed   = startFeedTask,
        .teardownMic = teardownMicrophone,
        .initMic     = initMicrophone,
        .feedRunning = &s_feedRunning,
        .feedsPerSec = &s_feedsPerSec,
    });

    // Register Gate 4 command window config
    sr_command_setConfig({
        .stopFeed    = stopFeedTask,
        .startFeed   = startFeedTask,
        .rxHandle    = &s_rxHandle,
        .feedRunning = &s_feedRunning,
        .feedsPerSec = &s_feedsPerSec,
        .showVisual  = cmdVisualHandler,
        .onCommand   = cmdActionHandler,
    });
#endif

    Serial.println("\n[READY] Say \"Lightwave\" to trigger wake word.");
    Serial.println("[READY] Commands: 'sr listen' (manual), or say wake word.");
    Serial.println("[READY] Dashboard: live VU meter + state + resources.\n");
}

static uint32_t lastDisplayMs = 0;
static uint32_t lastSerialMs = 0;
static uint32_t lastSoakMs = 0;
static String serialBuf;

void loop() {
    uint32_t now = millis();

    // --- Serial command handler ---
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            serialBuf.trim();
            if (serialBuf.length() > 0) {
                bool handled = false;
#ifdef ESP_SR_ENABLED
                handled = sr_lifecycle_handleCommand(serialBuf);
                if (!handled) handled = sr_command_handleCommand(serialBuf);
#endif
                if (!handled) {
                    Serial.printf("[CMD] Unknown: '%s'\n", serialBuf.c_str());
                }
            }
            serialBuf = "";
        } else {
            serialBuf += c;
        }
    }

#ifdef ESP_SR_ENABLED
    // Wake word triggered command window
    if (s_cmdWindowRequested) {
        s_cmdWindowRequested = false;
        // Drain any in-flight SPI display transactions before heavy PSRAM allocation
        zoneFlushAll();
        vTaskDelay(pdMS_TO_TICKS(20));
        runCommandWindow();
        now = millis();  // Refresh after blocking call
    }
#endif

    // Update display at ~15 FPS
    if (now - lastDisplayMs >= 66) {
        lastDisplayMs = now;
        updateDashboard();
    }

    // Serial amplitude log every 200ms
    if (now - lastSerialMs >= 200) {
        lastSerialMs = now;
        Serial.printf("[MIC] amp=%ld peak=%ld feeds/s=%d\n",
                      (long)s_micAmplitude, (long)s_micPeak, (int)s_feedsPerSec);
    }

    // Soak report every 30s
    if (now - lastSoakMs >= 30000) {
        lastSoakMs = now;
        printResources("SOAK");
    }

    delay(10);
}
